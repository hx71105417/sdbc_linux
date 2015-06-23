/***********************************************
 * Ïß³Ì³Ø·þÎñÆ÷ 
 ***********************************************/

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/resource.h>

#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/unistd.h>
#include <ctype.h>
#include <sdbc.h>

extern void set_showid(void *ctx);
extern srvfunc Function[];// appl function list
extern u_int family[];

srvfunc *SRVFUNC=Function;//used by get_srvname();
static int g_epoll_fd=-1;

// SDBC task control block for epoll event 
typedef struct event_node {
	struct event_node *next;
	int index;
	int events;
	int fd;
	T_Connect conn;
	T_NetHead head;
	char	   *ctx;
	int (*call_back)(T_Connect *conn,T_NetHead *head);
	int poolno;
	int status; //-1 Î´Á¬½Ó£¬0:Î´µÇÂ¼£¬1£ºÒÑµÇÂ¼
	INT64 timestamp;
} TCB;

static int do_epoll(TCB *task);

typedef  struct {
	pthread_mutex_t mut;
	pthread_cond_t cond;
	TCB *queue;
	int svc_num;
} Qpool;

//¾ÍÐ÷¶ÓÁÐ
static Qpool rpool={PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,NULL,-1};

//Ïß³Ì³Ø½Úµã
typedef struct {
	pthread_t tid;
	int status;
	INT64 timestamp;
} resource;
//Ïß³Ì³Ø
static struct {
	pthread_mutex_t mut;
	int num;
	resource *pool;
	pthread_attr_t attr;
} tpool={PTHREAD_MUTEX_INITIALIZER,0,NULL};
//ÈÎÎñ³Ø
static  struct {
	pthread_mutex_t mut;
	int max_client;
	TCB *pool;
} client_q={PTHREAD_MUTEX_INITIALIZER,0,NULL};

void TCB_add(TCB **rp,int TCBno)
{
TCB *en;
	if(TCBno<0 || TCBno>=client_q.max_client) return;
	en=&client_q.pool[TCBno];
	if(en->next) return;//²»¿ÉÒÔÔÚÆäËû¶ÓÁÐ
	if(!rp) {
		rp=&rpool.queue;
		pthread_mutex_lock(&rpool.mut);
	}
	if(!*rp) {
		*rp=en;
		en->next=en;
	} else {
		en->next=(*rp)->next;//Á¬½Ó¶ÓÍ·
		(*rp)->next=en;//¹Òµ½Á´Î²
		*rp=en;//Ö¸Ïòµ½Á´Î²
	}
	if(*rp==rpool.queue) {
		pthread_mutex_unlock(&rpool.mut);
		pthread_cond_signal(&rpool.cond); //»½ÐÑ¹¤×÷Ïß³Ì	
	}
}

int TCB_get(TCB **rp)
{
TCB *enp;
	if(!rp || !*rp) return -1;
	enp=(*rp)->next;//ÕÒµ½¶ÓÍ·
	if(enp->next == enp) *rp=NULL;//×îºóÒ»¸öÁË
	else (*rp)->next=enp->next;//ÐÂµÄ¶ÓÍ·
	enp->next=NULL;
	return enp->index;
}

void set_callback(int TCBno,int (*callback)(T_Connect *,T_NetHead *))
{
	if(TCBno<0 ||TCBno>client_q.max_client) return;
	client_q.pool[TCBno].call_back=callback;
}
/**
 * clr_event
 * Çå³ýÓÃ»§×Ô¶¨Òå»Øµ÷º¯Êýþ
 * @param TCB_no ¿Í»§ÈÎÎñºÅ
 * @return ³É¹¦ 0
 */
int unset_callback(int TCB_no)
{
        if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
        client_q.pool[TCB_no].call_back=NULL;
        return 0;
}



T_Connect *get_TCB_connect(int TCBno)
{
	if(TCBno<0 ||TCBno>client_q.max_client) return NULL;
	return &client_q.pool[TCBno].conn;
}

void *get_TCB_ctx(int TCBno)
{
	if(TCBno<0 ||TCBno>client_q.max_client) return NULL;
	return client_q.pool[TCBno].ctx;
}
void tpool_free()
{
int i;
	pthread_cond_destroy(&rpool.cond);
	pthread_mutex_destroy(&rpool.mut);
	pthread_mutex_destroy(&client_q.mut);
	if(client_q.pool) {
		if(client_q.pool[0].ctx) 
			free(client_q.pool[0].ctx);
		for(i=0;i<client_q.max_client;i++) {
			freeconnect(&client_q.pool[i].conn);
		}
		free(client_q.pool);
		client_q.pool=NULL;
	}
	client_q.max_client=0;

	pthread_mutex_destroy(&tpool.mut);
	if(tpool.pool) {
		free(tpool.pool);
		tpool.pool=NULL;
	}
	tpool.num=0;
	pthread_attr_destroy(&tpool.attr);
	if(g_epoll_fd > -1) close(g_epoll_fd);
	g_epoll_fd=-1;
	return ;
}

static int tpool_init(int size_ctx)
{
char *p;
int ret,i,limit;
int mtu,timeout;
struct rlimit sLimit;

	p=getenv("TIMEOUT");
        if(p && isdigit(*p)) {
                timeout=60*atoi(p);
        } else timeout=0;
	p=getenv("SENDSIZE");
        if(p && isdigit(*p)) {
                mtu=atoi(p);
        } else mtu=0;

	rpool.svc_num=-1;

	limit=getrlimit(RLIMIT_NOFILE,&sLimit);
	if(limit==0) {
		limit=sLimit.rlim_cur;
	}

	p=getenv("MAXCLT");
	if(!p || !isdigit(*p)) {
		ShowLog(4,"%s:È±ÉÙ»·¾³±äÁ¿MAXCLT,ÉèÖÃÎª2",__FUNCTION__);
		client_q.max_client=2;
	} else {
		client_q.max_client=atoi(p);
		if(limit>0) {
			i=(limit<<3)/10;
			if(client_q.max_client > i) client_q.max_client=i;
		}
	}
	ShowLog(2,"%s:MAXCLIENT=%d",__FUNCTION__,client_q.max_client);
	if(NULL==(client_q.pool=(TCB *)malloc((client_q.max_client+1) * sizeof(TCB)))) return -4;
	if(size_ctx>0)
		if(NULL==(client_q.pool[0].ctx=malloc((client_q.max_client+1) * size_ctx))) {
			free(client_q.pool);
			client_q.pool=NULL;
			return -2;
		} else ;
	else client_q.pool[0].ctx=NULL;
	
	for(i=0;i<=client_q.max_client;i++) {
		initconnect(&client_q.pool[i].conn);
		client_q.pool[i].next=NULL;
		client_q.pool[i].call_back=NULL;
		client_q.pool[i].index=i;
		client_q.pool[i].conn.timeout=timeout;
		client_q.pool[i].conn.MTU=mtu;
		client_q.pool[i].conn.family=family;
		client_q.pool[i].events=0;
		client_q.pool[i].poolno=0;
		client_q.pool[i].status=-1;
		if(!client_q.pool[0].ctx) client_q.pool[i].ctx=NULL;
		else if(i>0) client_q.pool[i].ctx=client_q.pool[0].ctx+i*size_ctx;
	}

	p=getenv("MAXTHREAD");
	if(!p || !isdigit(*p)) {
		ShowLog(4,"%s:È±ÉÙ»·¾³±äÁ¿MAXTHREAD,ÉèÖÃÎª1",__FUNCTION__);
		tpool.num=1;
	} else tpool.num=atoi(p);
	if(NULL==(tpool.pool=(resource *)malloc(tpool.num * sizeof(resource)))) {
		if(client_q.pool) {
			free(client_q.pool);
			client_q.pool=NULL;
		}
		return -3;
	}
	for(i=0;i<tpool.num;i++) {
		tpool.pool[i].tid=0;
		tpool.pool[i].status=0;
		tpool.pool[i].timestamp=0;
	}	
	ret= pthread_attr_init(&tpool.attr);
        if(ret) {
                ShowLog(1,"%s:can not init pthread attr %s",__FUNCTION__,strerror(ret));
        } else {
//ÉèÖÃ·ÖÀëÏß³Ì
        	ret=pthread_attr_setdetachstate(&tpool.attr,PTHREAD_CREATE_DETACHED);
        	if(ret) {
               	 ShowLog(1,"can't set pthread attr PTHREAD_CREATE_DETACHED:%s",strerror(ret));
       		}
//ÉèÖÃÏß³Ì¶ÑÕ»±£»¤Çø 256K
		pthread_attr_setguardsize(&tpool.attr,(size_t)(1024 * 256));
	}
	rpool.queue=NULL;
//ShowLog(5,"%s:maxfd=%d,maxclt=%d",__FUNCTION__,limit,client_q.max_client);
	if( 0 <= (g_epoll_fd=epoll_create(limit>0?limit<(client_q.max_client<<1)?limit:client_q.max_client<<1:client_q.max_client))) {
		return 0;
	}
	ShowLog(1,"%s:epoll_create err=%d,%s",
		__FUNCTION__,errno,strerror(errno));
	tpool_free();
	return SYSERR;
}

static void rdy_add(TCB *en)
{
	if(!rpool.queue) {
		rpool.queue=en;
		en->next=en;
	} else {
		en->next=rpool.queue->next;
		rpool.queue->next=en;
		rpool.queue=en;
	}
}

static TCB *rdy_get()
{
TCB *enp;
	if(!rpool.queue) return NULL;
	enp=rpool.queue->next;
	if(enp==NULL) {
		ShowLog(1,"%s:bad ready queue TCB:%d!",__FUNCTION__,rpool.queue->index);
		enp=rpool.queue;
		rpool.queue=NULL;
		return enp;
	}
	if(enp->next == enp) rpool.queue=NULL;
	else rpool.queue->next=enp->next;
	enp->next=NULL;
	return enp;
}
/**
 * set_event
 * ÓÃ»§×Ô¶¨ÒåÊÂ¼þ
 * @param TCB_no ¿Í»§ÈÎÎñºÅ
 * @param fd ÊÂ¼þfd Ö»Ö§³Ö¶ÁÊÂ¼þ 
 * @param call_back ·¢ÉúÊÂ¼þµÄ»Øµ÷º¯Êý
 * @param timeout fdµÄ³¬Ê±Ãë,Ö»ÔÊÐíÉèÖÃsocket fd
 * @return ³É¹¦ 0
 */
int set_event(int TCB_no,int fd,int (*call_back)(T_Connect *conn,T_NetHead *head),int timeout)
{
int ret;

	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	if(timeout>0) {
	struct timeval to;
		to.tv_sec=timeout;
          	to.tv_usec=0;
          	ret=setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,(char *)&to,sizeof(to));
           	if(ret) {
                   ShowLog(1,"%s:setsockopt RCVTIMEO %d error %s",__FUNCTION__,to.tv_sec,strerror(errno));
          	}            
	}
	client_q.pool[TCB_no].fd=fd;
	client_q.pool[TCB_no].call_back=call_back;
	client_q.pool[TCB_no].status=-1;
	ret=do_epoll(&client_q.pool[TCB_no]);
	client_q.pool[TCB_no].status=-5;
	return ret;
}

/**
 * clr_event
 * Çå³ýÓÃ»§×Ô¶¨ÒåÊÂ¼þ
 * @param TCB_no ¿Í»§ÈÎÎñºÅ
 * @return ³É¹¦ 0
 */
int clr_event(int TCB_no)
{
	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	client_q.pool[TCB_no].status=-4; //Ö»É¾³ýÊÂ¼þ 
	do_epoll(&client_q.pool[TCB_no]);

	client_q.pool[TCB_no].fd=client_q.pool[TCB_no].conn.Socket;
	client_q.pool[TCB_no].call_back=NULL;
	client_q.pool[TCB_no].status=1;
	return 0;
}

/**
 * get_event_fd
 * È¡ÊÂ¼þfd
 * @param TCB_no ¿Í»§ÈÎÎñºÅ
 * @return ÊÂ¼þfd
 */
int get_event_fd(int TCB_no)
{
	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	return client_q.pool[TCB_no].fd;
}
/**
 * get_event_status
 * È¡ÊÂ¼þ×´Ì¬
 * @param TCB_no ¿Í»§ÈÎÎñºÅ
 * @return ÊÂ¼þ×´Ì¬
 */
int get_event_status(int TCB_no)
{
	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	return client_q.pool[TCB_no].events;
}

/**
 * get_event_status
 * È¡TCB×´Ì¬
 * @param TCB_no ¿Í»§ÈÎÎñºÅ
 * @return TCB×´Ì¬
 */
int get_TCB_status(int TCB_no)
{
	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	return client_q.pool[TCB_no].status;
}

//¼ÓÈëÐÂÁ¬½Ó
static int do_epoll(TCB *task)
{
struct epoll_event epv = {0, {0}};
int  op,ret;

	epv.events =  EPOLLIN|EPOLLONESHOT;
	epv.data.ptr = task;
	task->timestamp=now_usec();
	switch(task->status) {
	case -1:
		op=EPOLL_CTL_ADD;
		break;
	case -3:
	case -4:
		op=EPOLL_CTL_DEL;
		break;
	case -5:// set_event() being it
		return 0;
	default:
		op=EPOLL_CTL_MOD;
		break;
	}
	if(!(task->events & EPOLLHUP)) {
	redo:
		ret=epoll_ctl(g_epoll_fd,op,task->fd,&epv);
		if(ret<0 && EEXIST == errno) {
//¿ÉÄÜÓÐµÄÏß³ÌÇÀÏÈÁË£¬µÈ±ðÈËÊÍ·ÅÕâ¸öfd
			usleep(1000);
			goto redo;
		}
	} else ret=0;
	if(ret<0 || op==EPOLL_CTL_DEL) {
		if(ret<0) ShowLog(1,"%s:tid=%lu,epoll_ctl fd[%d]=%d,ret=%d,err=%d,%s",__FUNCTION__,
			pthread_self(),task->index, task->fd,ret,errno,strerror(errno));
		else ShowLog(2,"%s:tid=%lu epoll_ctl fd[%d]=%d,deleted",__FUNCTION__,pthread_self(),
			task->index, task->fd);
		if(task->status == -3) {
			pthread_mutex_lock(&tpool.mut);
			freeconnect(&task->conn);
			task->events=0;
			task->status=-1;
			pthread_mutex_unlock(&tpool.mut);
		}
		if(ret) return ret;
	}
	return 0;
}
//¹¤×÷Ïß³Ì
static void *thread_work(void *param)
{
resource *rs=(resource *)param;
TCB *task;
T_Connect *conn;
int ret,cc;
void (*init)(T_Connect *,T_NetHead *);
T_SRV_Var ctx;

	rs->tid=pthread_self();
	ShowLog(4,"%s:thread%lu crested,TIMEVAL=%d",__FUNCTION__,rs->tid,(int)(now_usec() - rs->timestamp));
	ctx.tid=rs->tid;//±êÖ¾¶àÏß³Ì·þÎñ
	ctx.SQL_Connect.dbh=-1;
	ctx.SQL_Connect.tid=NULL;

	while(1) {
//´Ó¾ÍÐ÷¶ÓÁÐÈ¡Ò»¸öÈÎÎñ
		rs->timestamp=now_usec();

		pthread_mutex_lock(&rpool.mut);
		do {
			task=rdy_get();
			if(!task) {
			struct timespec tim;
				pthread_mutex_lock(&tpool.mut);
				rs->status=0;
				pthread_mutex_unlock(&tpool.mut);
				gettimeofday((struct timeval *)&tim,0);
				tim.tv_sec+=300; //µÈ´ý5·ÖÖÓ 
				tim.tv_nsec*=1000;
				ret=pthread_cond_timedwait(&rpool.cond,&rpool.mut,&tim); //Ã»ÓÐÈÎÎñ£¬µÈ´ý
				if(ETIMEDOUT == ret)  break;
				pthread_mutex_lock(&tpool.mut);
				rs->status=1;
				pthread_mutex_unlock(&tpool.mut);
			}
		} while(!task);
		pthread_mutex_unlock(&rpool.mut);
		if(!task) break;
		ctx.TCB_no=task->index;
		ret=0;
		task->timestamp=
		rs->timestamp=now_usec();
		conn=&task->conn;
		task->conn.Var=&ctx;
		ctx.var=task->ctx;
//ShowLog(5,"%s:got TCB %d,status=%d",__FUNCTION__,task->index,task->status);
	      if(!task->call_back) { //SDBC±ê×¼ÊÂ¼þ 
//Ð­ÉÌÃÜÔ¿
		if(task->status==-1) {
			init=(void (*)())conn->only_do;
			task->conn.Var=&ctx;
			ctx.var=task->ctx;
			conn->only_do=0;
			if(init) init(conn,&task->head);
			ret=mk_clikey(conn->Socket,&conn->t,conn->family);
			if(ret<0) {
				ShowLog(1,"%s:tid=%lu task[%d]Ð­ÉÌÃÜÔ¿Ê§°Ü!",__FUNCTION__,ctx.tid,task->index);
//ÊÍ·ÅÁ¬½Ó
errret:
				task->status=-3; //delete it
			} else {
				conn->CryptFlg=ret;
				task->status=0;
			}
//ShowLog(5,"%s:aft clikey TCB:%d",__FUNCTION__,task->index);
			ret=do_epoll(task);
			mthr_showid_del(ctx.tid);
			continue;
		}

		ctx.poolno=task->poolno;
		if(task->status>0) set_showid(task->ctx);//Showid Ó¦¸ÃÔÚ»á»°ÉÏÏÂÎÄ½á¹¹Àï 
//ShowLog(5,"%s:tid=%lu,begin RecvPack!",__FUNCTION__,ctx.tid);
		ret=RecvPack(conn,&task->head);
		if(ret) {
			ShowLog(1,"%s:task[%d]½ÓÊÕ´íÎó,tid=%lu,err=%d,%s",__FUNCTION__,
				task->index,rs->tid,errno,strerror(errno));
			goto errret;
		}
		ShowLog(4,"%s: tid=%lu,TCB:%d,PROTO_NUM:%d len:%d",__FUNCTION__,ctx.tid,
                        task->index,task->head.PROTO_NUM,task->head.PKG_LEN);

		if(task->head.PROTO_NUM==65535) {
			ShowLog(2,"%s: disconnect by client",__FUNCTION__);
			goto errret;
		} else if(task->head.PROTO_NUM==1){
                        Echo(conn,&task->head);
                } else if(task->status==0) {
			ret=Function[0].funcaddr(conn,&task->head);
			if(ret==-1) task->status=-3;
			else if(ret!=-5) task->status=ret;
			if(task->status==1) {
				task->poolno=ctx.poolno;
			}
		} else if (conn->only_do) {
			ret=conn->only_do(conn,&task->head);
			if(ret==-1) task->status=-3;
			
		} else {
			if(task->head.PROTO_NUM==0) {
				ret=get_srvname(conn,&task->head);
			} else if(task->head.PROTO_NUM>rpool.svc_num) {
                                ShowLog(1,"%s:Ã»ÓÐÕâ¸ö·þÎñºÅ %d",__FUNCTION__,task->head.PROTO_NUM);
                                task->status=-3;
                        } else ret=Function[task->head.PROTO_NUM].funcaddr(conn,&task->head);
		}
	      } else { //ÓÃ»§×Ô¶¨ÒåÊÂ¼þ
		ctx.poolno=task->poolno;
		set_showid(task->ctx);//Showid Ó¦¸ÃÔÚ»á»°ÉÏÏÂÎÄ½á¹¹Àï 
		ret=task->call_back(conn,&task->head);
//ShowLog(5,"%s:aft call_back TCB:%d,ret=%d,task->status=%d",__FUNCTION__,task->index,ret,task->status);
		if(task->status==0) { //finish_login,return 0 or 1
			task->status=ret;
			if(ret==1) task->poolno=ctx.poolno;
		}
	      }
//»ØµÈ´ý¶ÓÁÐ
//ShowLog(5,"%s:befor do_epoll TCB:%d,return=%d,status=%d",__FUNCTION__,task->index,ret,task->status);
		switch(ret) {
		case -1:
			ret=task->status;
			ShowLog(2,"%s: disconnect by server",__FUNCTION__);
			task->status=-3;
			break;
		case -5:
			ret=task->status;
			task->status=-5;
			break;
		default:
			ret=task->status;
			break;
		}
		
		cc=do_epoll(task);
		task->status=ret;
//ShowLog(5,"%s:aft do_epoll TCB:%d,ret=%d,task->status=%d",__FUNCTION__,task->index,ret,task->status);
		mthr_showid_del(ctx.tid);
	}
	ShowLog(3,"%s:tid=%lu canceled",__FUNCTION__,pthread_self());
	mthr_showid_del(ctx.tid);
	rs->timestamp=now_usec();
	rs->status=0;
	rs->tid=0;
	return NULL;
}

static int test_wt()
{
int i,n=-2;

	pthread_mutex_lock(&tpool.mut);
	for(i=0;i<tpool.num;i++) {
//ShowLog(5,"%s:tpool[%d].tid=%lu,status=%d",__FUNCTION__,i,tpool.pool[i].tid,tpool.pool[i].status);
		if(!tpool.pool[i].tid) {
			if(n<0) n=i;
		} else if(tpool.pool[i].tid==-1) {
			break;
		} else if (tpool.pool[i].status==0) {
			n=-1;
			break;
		}
	}
	pthread_mutex_unlock(&tpool.mut);
	return n;
}
//¼ì²é³¬Ê±µÄÁ¬½Ó
int check_TCB_timeout()
{
int i;
TCB * tp;
INT64 now=now_usec();
int num=0;

	for(i=0;i<client_q.max_client;i++) {
		tp=&client_q.pool[i];
		if(tp->conn.Socket > -1 && tp->events==0 && tp->conn.timeout>0) {
		int t=(int)((now-tp->timestamp)/1000000);
			if(t>tp->conn.timeout) {
				tp->status=-3;	//delete it
				do_epoll(tp);
				num++;
			}
		}
	}
	return num;
}

//½¨Á¢ÐÂÏß³Ì
static int new_wt(int n)
{
pthread_t tid;
int ret;

	if(n<0) return n;
	tpool.pool[n].status=1;
	tpool.pool[n].timestamp=now_usec();
	tpool.pool[n].tid=-1;
	ret=pthread_create(&tid,&tpool.attr,thread_work,&tpool.pool[n]);
        if(ret) {
		tpool.pool[n].tid=0;
                ShowLog(1,"%s:pthread_create:%s",__FUNCTION__,strerror(ret));
		return ret;
        }
//	while(!pthread_equal(tpool.pool[n].tid,tid)) usleep(1000);
	ShowLog(5,"%s:tid[%d]=%lu to be create",__FUNCTION__,n,tid);
	return 0;
}
/**
 * µ÷¶ÈÏß³Ì
 */

static void * sched(void *param)
{
int fds,i,ret;
struct epoll_event events[tpool.num];

	ShowLog(2,"%s:tid=%lu start",__FUNCTION__,pthread_self());
	while(1) {
		fds = epoll_wait(g_epoll_fd, events, tpool.num , 30000);
		if(fds < 0){
       	 		ShowLog(1,"%s:epoll_wait err=%d,%s",__FUNCTION__,errno,strerror(errno));
			sleep(30);
			continue;
       	 	}
		if(fds==0) { //³¬Ê±ÁË	
			check_TCB_timeout();
			continue;
		}
		for(i=0;i<fds;i++) {
		 TCB *task;
		 	task = (TCB *)events[i].data.ptr;
//¼ì²éÊÂ¼þÊÇ·ñ½¡¿µ
//			if(task->conn.Socket<0) continue;	//±£ÏÕÒ»µã
			task->events=events[i].events;
			if(!task->call_back && !(events[i].events&EPOLLIN)) {
				task->status=-3; //delete it	
				do_epoll(task);
				continue;
			}
			pthread_mutex_lock(&rpool.mut);
			rdy_add(task);
			ret=test_wt(); //²âÊÔ¹¤×÷Ïß³Ì
//ShowLog(5,"%s:TCB:%d,test_wt=%d,event=%08X,i=%d",__FUNCTION__,task->index,ret,events[i].events,i);
			pthread_mutex_unlock(&rpool.mut);
			if(ret==-1) pthread_cond_signal(&rpool.cond); //»½ÐÑ¹¤×÷Ïß³Ì	
			else new_wt(ret); //½¨Á¢ÐÂÏß³Ì
		}
	}
	close(g_epoll_fd);
	g_epoll_fd=-1;
	return NULL;
}

static int event_no()
{
int i;
	pthread_mutex_lock(&client_q.mut);
	for(i=0;i<client_q.max_client;i++) 
		if(client_q.pool[i].conn.Socket==-1) break;
	if(i==client_q.max_client) {
		pthread_mutex_unlock(&client_q.mut);
		return -1;
	}
	client_q.pool[i].poolno=0;
	client_q.pool[i].next=NULL;
	client_q.pool[i].conn.Socket=-2; //ÏÈÕ¼Î»
	pthread_mutex_unlock(&client_q.mut);
	return i;
}

int TPOOL_srv(void (*conn_init)(T_Connect *,T_NetHead *),void (*quit)(int),void (*poolchk)(void),int sizeof_gda)
{
int ret,i;
int s;
struct sockaddr_in sin,cin;
struct servent *sp;
char *p;
pthread_t pthread_id;
pthread_attr_t attr;
struct timeval tm;
fd_set efds;
socklen_t leng=1;
int sock=-1;
srvfunc *fp;

	tzset();

	ret= pthread_attr_init(&attr);
	if(ret) {
		ShowLog(1,"%s:can not init pthread attr %s",__FUNCTION__,strerror(ret));
		return -1;
	}
//ÉèÖÃ·ÖÀëÏß³Ì  
	ret=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	if(ret) {
		ShowLog(1,"%s:can't set pthread attr:%s",__FUNCTION__,strerror(ret));
		return -2;
	}

	signal(SIGPIPE,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	signal(SIGINT ,SIG_IGN);
	signal(SIGPWR ,quit);
	signal(SIGTERM,quit);

	p=getenv("SERVICE");
	if(!p || !*p) {
		ShowLog(1,"È±ÉÙ»·¾³±äÁ¿ SERVICE ,²»ÖªÊØºòÄÄ¸ö¶Ë¿Ú£¡");
		quit(3);
	}
//²âÊÔ¶Ë¿ÚÊÇ·ñ±»Õ¼ÓÃ 
	sock=tcpopen("localhost",p);
	if(sock>-1) {
		ShowLog(1,"¶Ë¿Ú %s ÒÑ¾­±»Õ¼ÓÃ",p);
		close(sock);
		sock=-1;
		quit(255);
	}

	ret=tpool_init(sizeof_gda);
	if(ret) return(ret);

	for(fp=Function;fp->funcaddr!=0;fp++) rpool.svc_num++;
	bzero(&sin,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	if(isdigit(*p)){
		sin.sin_port=htons((u_short)atoi(p));
	} else {
		if((sp=getservbyname(p,"tcp"))==NULL){
        		ShowLog(1,"getsrvbyname %s error",p);
        		quit(3);
		}
		sin.sin_port=(u_short)sp->s_port;
	}

	sock=socket(AF_INET,SOCK_STREAM,0);
	if(sock < 0) {
		ShowLog(1,"open socket error=%d,%s",errno,
			strerror(errno));
		quit(3);
	}

	bind(sock,(struct sockaddr *)&sin,sizeof(sin));
//#ifdef LINUX
	leng=1;
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&leng,sizeof(leng));
//#endif
	listen(sock,client_q.max_client>>1);

// Æô¶¯µ÷¶ÈÏß³Ì	
	ret=pthread_create(&pthread_id,&attr,sched,NULL);
	if(ret) {
		ShowLog(1,"%s:create sched thread fail!err=%d,%s",
			__FUNCTION__,ret,strerror(ret));
		close(sock);
		tpool_free();
		return -1;
	}
	ShowLog(0,"main start tid=%lu sock=%d",pthread_self(),sock);
	
	int repeat=0;
	leng=sizeof(cin);

	while(1) {
		do {
			FD_ZERO(&efds);
			FD_SET(sock, &efds);
//½¡¿µ¼ì²éÖÜÆÚ5·ÖÖÓ 
			tm.tv_sec=300;
			tm.tv_usec=0;
			ret=select(sock+1,&efds,NULL,&efds,&tm);
			if(ret==-1) {
				ShowLog(1,"select error %s",strerror(errno));
				close(sock);
				quit(3);
			}
			if(ret==0 && poolchk) poolchk();
		} while(ret<=0);

		while(0>(i=event_no())) {
			ShowLog(1,"%s:³¬¹ý×î´óÁ¬½ÓÊý£¡",__FUNCTION__);
			ret=check_TCB_timeout(); //²éÒ»ÏÂÓÐÃ»ÓÐ³¬Ê±ÍË³öµÄ
			if(ret <= 0) sleep(10);
		}

		s=accept(sock,(struct sockaddr *)&cin,&leng);
		if(s<0) {
			ShowLog(1,"%s:accept err=%d,%s",__FUNCTION__,errno,strerror(errno));
			switch(errno) {
			case EMFILE:	//fdÓÃÍêÁË,ÆäËûÏß³Ì»¹Òª¼ÌÐø¹¤×÷£¬Ö÷Ïß³ÌÐÝÏ¢Ò»ÏÂ¡£  
			case ENFILE:
				sleep(30);
				continue;
			default:break;
			}
			sleep(15);
			if(++repeat < 20) continue;
			ShowLog(1,"%s:network fail! err=%s",__FUNCTION__,strerror(errno));
			close(sock);
			quit(5);
		}
		repeat=0;
		client_q.pool[i].fd=s;
		client_q.pool[i].conn.Socket=s;
		client_q.pool[i].conn.timeout=120;
		client_q.pool[i].status=-1;
		client_q.pool[i].conn.only_do=(int (*)())conn_init;
		ret=do_epoll(&client_q.pool[i]);
	}
	
	close(sock);
	pthread_attr_destroy(&attr);
	tpool_free();
	return (0);
}
