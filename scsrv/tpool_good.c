/***********************************************
 * 线程池服务器 
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
#include <scsrv.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C"
#endif
extern void set_showid(void *ctx);

extern srvfunc Function[];// appl function list
extern u_int family[];
static void *thread_work(void *param);

static int g_epoll_fd=-1;

// SDBC task control block for epoll event 
typedef struct event_node {
	struct event_node *next;
	int events;
	int fd;
	T_Connect conn;
	T_NetHead head;
	T_SRV_Var sv;
	char	   *ctx;
	ucontext_t uc;
	sdbcfunc call_back;
	INT64 timestamp;
	int timeout;
	volatile int status; //-1 未连接，0:未登录，1：已登录
} TCB;

static int do_epoll(TCB *task,int op,int flg);

typedef  struct {
	pthread_mutex_t mut;
	pthread_cond_t cond;
	TCB *queue;
	int svc_num;
	char flg;	//无守护线程0，有1
} Qpool;

//就绪队列
static Qpool rpool={PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,NULL,-1,0};

//线程池节点
typedef struct {
	pthread_t tid;
	int status;
	ucontext_t tc;
	INT64 timestamp;
} resource;
//线程池
static struct {
	pthread_mutex_t mut;
	int num;
	int rdy_num;
	resource *pool;
	pthread_attr_t attr;
} tpool={PTHREAD_MUTEX_INITIALIZER,0,1,NULL};
//任务池
static  struct {
	pthread_mutex_t mut;
	pthread_cond_t cond;
	int max_client;
	TCB *pool;
	TCB *free_q;
} client_q={PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,0,NULL,NULL};

T_SRV_Var * get_SRV_Var(int TCBno)
{
	if(TCBno<0 || TCBno>=client_q.max_client) return NULL;
	return &client_q.pool[TCBno].sv;
}

void TCB_add(TCB **rp,int TCBno)
{
TCB *en;
	if(TCBno<0 || TCBno>=client_q.max_client) return;
	en=&client_q.pool[TCBno];
//	en->timestamp=now_usec();
	if(en->next) {
		ShowLog(1,"%s:TCB:%d 已经在队列中",__FUNCTION__,TCBno);
		return;//不可以在其他队列
	}
	if(!rp) {
		pthread_mutex_lock(&rpool.mut);
		rp=&rpool.queue;
	}
	if(!*rp) {
		*rp=en;
		en->next=en;
	} else {
		en->next=(*rp)->next;//连接队头
		(*rp)->next=en;//挂到链尾
		*rp=en;//指向到链尾
	}
	if(*rp==rpool.queue) {
		pthread_mutex_unlock(&rpool.mut);
//ShowLog(1,"%s:TCB:%d,tid=%lX",__FUNCTION__,TCBno,pthread_self());
		pthread_cond_signal(&rpool.cond); //唤醒工作线程	
	}
}

int TCB_get(TCB **rp)
{
TCB *enp;
	if(!rp || !*rp) return -1;
	enp=(*rp)->next;//找到队头
	if(!enp->next) {
		ShowLog(1,"%s:TCB=%d,错，未在队列中！",__FUNCTION__,enp->conn.TCB_no);
		return -2;
	}
	if(enp->next == enp) *rp=NULL;//最后一个了
	else (*rp)->next=enp->next;//新的队头
	enp->next=NULL;
	return enp->sv.TCB_no;
}

sdbcfunc  set_callback(int TCBno,sdbcfunc callback,int timeout)
{
sdbcfunc old;
TCB *task;

	if(TCBno<0 ||TCBno>client_q.max_client) return (sdbcfunc)-1;
	task=&client_q.pool[TCBno];
	task->timeout=timeout;
	old=task->call_back;
	task->call_back=callback;
	return old;
}
/**
 * unset_callback
 * 清除用户自定义回调函数�
 * @param TCB_no 客户任务号
 * @return 原回调函数
 */
sdbcfunc unset_callback(int TCB_no)
{
sdbcfunc old;
TCB *task;
        if(TCB_no<0 || client_q.max_client <= TCB_no) return NULL;
	task=&client_q.pool[TCB_no];
	old=task->call_back;
        task->call_back=NULL;
        task->timeout=task->conn.timeout;
	
        return old;
}

/**
 * get_callback
 * 取用户自定义回调函数�
 * @param TCB_no 客户任务号
 * @return 回调函数
 */
sdbcfunc get_callback(int TCB_no)
{
        if(TCB_no<0 || client_q.max_client <= TCB_no) return NULL;
	return client_q.pool[TCB_no].call_back;
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
	if(client_q.pool) {
		if(client_q.pool[0].ctx) 
			free(client_q.pool[0].ctx);
		for(i=0;i<client_q.max_client;i++) {
			client_q.pool[i].conn.Var=NULL;
			freeconnect(&client_q.pool[i].conn);
		}
		free(client_q.pool);
		client_q.pool=NULL;
		client_q.free_q=NULL;
	}
	client_q.max_client=0;

	rpool.svc_num=-1;
	rpool.flg=0;

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

//建立新线程
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
	return 0;
}
extern srvfunc *SRVFUNC;
static int tpool_init(int size_ctx)
{
char *p;
int ret,i,limit;
int mtu,timeout;
struct rlimit sLimit;
TCB *task;

	p=getenv("TIMEOUT");
        if(p && isdigit(*p)) {
                timeout=60*atoi(p);
        } else timeout=0;
	p=getenv("SENDSIZE");
        if(p && isdigit(*p)) {
                mtu=atoi(p);
        } else mtu=0;

	rpool.svc_num=-1;
	SRVFUNC=Function;//used by get_srvname();

	limit=getrlimit(RLIMIT_NOFILE,&sLimit);
	if(limit==0) {
		limit=sLimit.rlim_cur;
	}

	p=getenv("MAXCLT");
	if(!p || !isdigit(*p)) {
		ShowLog(4,"%s:缺少环境变量MAXCLT,设置为2",__FUNCTION__);
		client_q.max_client=2;
	} else {
		client_q.max_client=atoi(p);
		if(limit>0) {
			i=(limit<<3)/10;
			if(client_q.max_client > i) client_q.max_client=i;
		}
	}
	ShowLog(0,"%s:MAXCLIENT=%d",__FUNCTION__,client_q.max_client);
	if(NULL==(client_q.pool=(TCB *)malloc((client_q.max_client+1) * sizeof(TCB)))) return -4;
	if(size_ctx>0)
		if(NULL==(client_q.pool[0].ctx=malloc((client_q.max_client+1) * size_ctx))) {
			free(client_q.pool);
			client_q.pool=NULL;
			return -2;
		} else ;
	else client_q.pool[0].ctx=NULL;
	client_q.free_q=NULL;
	
	task=client_q.pool;
	for(i=0;i<=client_q.max_client;i++,task++) {
		initconnect(&task->conn);
		task->next=NULL;
		task->call_back=NULL;
		task->sv.TCB_no=i;
		task->timeout=0;
		task->conn.timeout=timeout;
		task->conn.MTU=mtu;
		task->conn.family=family;
		task->events=0;
		task->sv.poolno=-1;
		task->sv.SQL_Connect=NULL;
		task->status=-1;
		getcontext(&task->uc);
		task->uc.uc_stack.ss_flags=0;
		task->uc.uc_stack.ss_size=0;
		task->uc.uc_stack.ss_sp=NULL;
		task->uc.uc_link=NULL;
		if(!client_q.pool[0].ctx) task->ctx=NULL;
		else if(i>0) task->ctx=client_q.pool[0].ctx+i*size_ctx;
		task->sv.var=task->ctx;
		TCB_add(&client_q.free_q,i);
	}

	p=getenv("RDY_NUM");
	if(p && isdigit(*p)) tpool.rdy_num=atoi(p);
	else tpool.rdy_num=1;
	p=getenv("MAXTHREAD");
	if(!p || !isdigit(*p)) {
		tpool.num=tpool.rdy_num+1;
		ShowLog(4,"%s:缺少环境变量MAXTHREAD,设置为%d",__FUNCTION__,tpool.num);
	} else tpool.num=atoi(p);
	if(NULL==(tpool.pool=(resource *)malloc(tpool.num * sizeof(resource)))) {
		if(client_q.pool) {
			free(client_q.pool);
			client_q.pool=NULL;
		}
		return -3;
	}
	ret= pthread_attr_init(&tpool.attr);
        if(ret) {
                ShowLog(1,"%s:can not init pthread attr %s",__FUNCTION__,strerror(ret));
        } else {
//设置分离线程
        	ret=pthread_attr_setdetachstate(&tpool.attr,PTHREAD_CREATE_DETACHED);
        	if(ret) {
               	 ShowLog(1,"%s:can't set pthread attr PTHREAD_CREATE_DETACHED:%s",
		 	__FUNCTION__,strerror(ret));
       		}
//设置线程堆栈保护区 256K
		pthread_attr_setguardsize(&tpool.attr,(size_t)(1024 * 256));
	}
	for(i=0;i<tpool.num;i++) {
		tpool.pool[i].tid=0;
		tpool.pool[i].status=0;
		getcontext(&tpool.pool[i].tc);
		tpool.pool[i].tc.uc_stack.ss_flags=0;
		tpool.pool[i].tc.uc_stack.ss_size=0;
		tpool.pool[i].tc.uc_stack.ss_sp=NULL;
		tpool.pool[i].tc.uc_link=NULL;
	}	
	rpool.queue=NULL;
//ShowLog(5,"%s:maxfd=%d,maxclt=%d",__FUNCTION__,limit,client_q.max_client);
	if( 0 <= (g_epoll_fd=epoll_create(limit>0?limit<(client_q.max_client<<1)?limit:client_q.max_client<<1:client_q.max_client))) {
		for(i=0;i<tpool.num;i++) new_wt(i);
		return 0;
	}
	ShowLog(1,"%s:epoll_create err=%d,%s",
		__FUNCTION__,errno,strerror(errno));
	tpool_free();
	return SYSERR;
}
/*
static void rdy_add(TCB *en)
{
	if(en->next) { //TCB 已经在队列中
		ShowLog(1,"%s:TCB:%d 已经在队列中",__FUNCTION__,en->conn.TCB_no);
		return;
	}
	if(!rpool.queue) {
		rpool.queue=en;
		en->next=en;
	} else {
		en->next=rpool.queue->next;
		rpool.queue->next=en;
		rpool.queue=en;
	}
}
*/
static TCB *rdy_get()
{
TCB *enp;
	if(!rpool.queue) return NULL;
	enp=rpool.queue->next;
	if(enp==NULL) {
		ShowLog(1,"%s:bad ready queue TCB:%d!",__FUNCTION__,rpool.queue->conn.TCB_no);
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
 * 用户自定义事件
 * @param TCB_no 客户任务号
 * @param fd 事件fd 只支持读事件 
 * @param call_back 发生事件的回调函数
 * @param timeout fd的超时秒,只允许设置socket fd
 * @return 成功 0
 */
int set_event(int TCB_no,int fd,sdbcfunc call_back,int timeout)
{
TCB *task;

	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	task=&client_q.pool[TCB_no];
	task->fd=fd;
	task->call_back=call_back;
	task->timestamp=now_usec();
	task->timeout=timeout;
	return do_epoll(task,(task->fd==task->conn.Socket)?EPOLL_CTL_MOD:EPOLL_CTL_ADD,0);
}

/**
 * clr_event
 * 清除用户自定义事件
 * @param TCB_no 客户任务号
 * @return 成功 0
 */
int clr_event(int TCB_no)
{
TCB *task;
int ret;
	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	task=&client_q.pool[TCB_no];
	if(task->fd == task->conn.Socket) {
		task->call_back=NULL;
		task->status=1;
		return -2;
	}
	task->call_back=NULL;
	ret=do_epoll(task,EPOLL_CTL_DEL,0);
	if(ret) ShowLog(1,"%s:tid=%lX,TCB:%d,do_epoll ret=%d",
		__FUNCTION__,pthread_self(),TCB_no,ret);
	task->fd=task->conn.Socket;
	task->timeout=task->conn.timeout;
	return 0;
}

/**
 * get_event_fd
 * 取事件fd
 * @param TCB_no 客户任务号
 * @return 事件fd
 */
int get_event_fd(int TCB_no)
{
	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	return client_q.pool[TCB_no].fd;
}
/**
 * get_event_status
 * 取事件状态
 * @param TCB_no 客户任务号
 * @return 事件状态
 */
int get_event_status(int TCB_no)
{
	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	return client_q.pool[TCB_no].events;
}

/**
 * get_event_status
 * 取TCB状态
 * @param TCB_no 客户任务号
 * @return TCB状态
 */
int get_TCB_status(int TCB_no)
{
	if(TCB_no<0 || client_q.max_client <= TCB_no) return -1;
	return client_q.pool[TCB_no].status;
}

static void client_del(TCB *task)
{
int status=task->status;
struct linger so_linger;

	task->fd=-1;
	so_linger.l_onoff=1;
        so_linger.l_linger=0;
        setsockopt(task->conn.Socket, SOL_SOCKET, SO_LINGER, &so_linger, sizeof so_linger);
	pthread_mutex_lock(&tpool.mut);
	if(task->uc.uc_stack.ss_sp) {
		free(task->uc.uc_stack.ss_sp);
		task->uc.uc_stack.ss_sp=NULL;
	}
	task->uc.uc_stack.ss_size=0;
	freeconnect(&task->conn);
	task->events=0;
	task->status=-1;
	task->timeout=0;
	pthread_mutex_unlock(&tpool.mut);
	pthread_mutex_lock(&client_q.mut);
	TCB_add(&client_q.free_q,task->conn.TCB_no);
	pthread_mutex_unlock(&client_q.mut);
	pthread_cond_signal(&client_q.cond); //唤醒主线程	
	if(status>-1) ShowLog(3,"%s:tid=%lX,TCB:%d deleted!",__FUNCTION__,pthread_self(),task->conn.TCB_no);
}

//加入新连接
static int do_epoll(TCB *task,int op,int flg)
{
struct epoll_event epv = {0, {0}};
int  status,ret;
	if(task->fd<0) return FORMATERR;
	if(task->next) {
		ShowLog(1,"%s:tid=%lX,TCB:%d 已经在队列中,fd=%d,Sock=%d",__FUNCTION__,
			pthread_self(),task->conn.TCB_no,task->fd,task->conn.Socket);
		return -1;
	}
	status=task->status;
	epv.events =  flg?EPOLLOUT:EPOLLIN;
	epv.events |= EPOLLONESHOT;
	epv.data.ptr = task;
	task->events=0;
	if(op) 
		ret=epoll_ctl(g_epoll_fd,op,task->fd,&epv);
	else {
		ret=epoll_ctl(g_epoll_fd,EPOLL_CTL_MOD,task->fd,&epv);
		if(ret < 0 && errno==ENOENT) 
			ret=epoll_ctl(g_epoll_fd,EPOLL_CTL_ADD,task->fd,&epv);
	}
	if(ret<0 || op==EPOLL_CTL_DEL) {
		if(ret<0) {
			if( errno != EEXIST) ShowLog(1,"%s:tid=%lX,epoll_ctl fd[%d]=%d,op=%d,ret=%d,err=%d,%s",__FUNCTION__,
				pthread_self(),task->conn.TCB_no, task->fd,op,ret,errno,strerror(errno));
		} else {
			task->fd=-1;
			if(task->status>-1) 
			   ShowLog(3,"%s:tid=%lX epoll_ctl fd[%d]=%d,deleted,op=%d",__FUNCTION__,
				pthread_self(),task->conn.TCB_no, task->fd,op);
		}
	}
	return ret;
}

int do_event(int TCBno,int sock,int flg)
{
TCB *task;
int save_fd;
int ret;

	if(TCBno<0 ||TCBno>client_q.max_client) return -1;
	task=&client_q.pool[TCBno];
	save_fd=task->fd;
	task->fd=sock;
	ret=do_epoll(task,0,flg);
	task->fd=save_fd;
	return ret;
}

//工作线程
static int do_work(TCB *task)
{
int ret;
T_Connect *conn;
void (*init)(T_Connect *,T_NetHead *);
T_SRV_Var *ctx=&task->sv;

	ctx->tid=pthread_self();//标志多线程服务
	conn=&task->conn;
	conn->Var=ctx;
	conn->TCB_no=ctx->TCB_no;
	ret=0;
	if(!task->call_back) { //SDBC标准事件 
//协商密钥
		if(task->status==-1) {
			init=(void (*)())conn->only_do;
			conn->only_do=0;
			ret=mk_clikey(conn->Socket,&conn->t,conn->family,conn->TCB_no);
			if(ret<0) {
				if(ret!=SYSERR) { 
				char addr[16];
					peeraddr(conn->Socket,addr);
					ShowLog(1,"%s:tid=%lX,TCB:%d,协商密钥失败!,addr=%s,ret=%d",__FUNCTION__,
							ctx->tid,task->conn.TCB_no,addr,ret);
				}
	//释放连接
				return -1;
			} 
			conn->CryptFlg=ret;
			task->status=0;
			if(init) init(conn,&task->head);
			task->timeout=60;//60秒内必须登录
			return 0;
		}
	
		if(task->status>0) set_showid(task->ctx);
		ret=RecvPack(conn,&task->head);
		task->timestamp=now_usec();
		if(ret) {
			ShowLog(1,"%s:TCB:%d,接收错误,tid=%lX,err=%d,%s,event=%08X",__FUNCTION__,
				task->conn.TCB_no,ctx->tid,errno,strerror(errno),task->events);
			return -1;
		}
		ShowLog(4,"%s: tid=%lX,TCB:%d,PROTO_NUM=%d pkg_len=%d,t_len=%d,USEC=%llu",
			__FUNCTION__,ctx->tid,task->conn.TCB_no,
	                task->head.PROTO_NUM,task->head.PKG_LEN,task->head.T_LEN,
			task->timestamp);
	
		if(task->head.PROTO_NUM==65535) {
			ShowLog(3,"%s: disconnect by client",__FUNCTION__);
			return -1;
		} else if(task->head.PROTO_NUM==1){
	                ret=Echo(conn,&task->head);
	        } else if(task->status==0) {
			if(!task->head.PROTO_NUM) {
				ret=Function[0].funcaddr(conn,&task->head);
				if(ret>=0) {
					task->status=ret;
					if(ret==1) task->timeout=conn->timeout;
				}
	                } else {
	                        ShowLog(1,"%s:TCB:%d,未登录",__FUNCTION__,task->conn.TCB_no);
	                        return -1;
	                }
		} else if (conn->only_do) {
			ret=conn->only_do(conn,&task->head);
		} else {
			if(task->head.PROTO_NUM==0) {
				ret=get_srvname(conn,&task->head);
			} else if(task->head.PROTO_NUM>rpool.svc_num) {
	                         ShowLog(1,"%s:没有这个服务号 %d",__FUNCTION__,task->head.PROTO_NUM);
	                         ret=-1;
	                } else {
	                        ret=Function[task->head.PROTO_NUM].funcaddr(conn,&task->head);
	                        if(ret==-1)
					ShowLog(1,"%s:TCB:%d,disconnect by server",__FUNCTION__,
						task->conn.TCB_no);
			}
		}
	} else { //用户自定义事件
		task->timestamp=now_usec();
		set_showid(task->ctx);//Showid 应该在会话上下文结构里 
		ShowLog(5,"%s:call_back tid=%lX,USEC=%llu",__FUNCTION__,ctx->tid,task->timestamp);
		ret=task->call_back(conn,&task->head);
		if(task->status==0)  //finish_login,return 0 or 1
			task->status=ret;
	        if(ret==-1)
			ShowLog(1,"%s:TCB:%d,disconnect by server",__FUNCTION__,
				task->conn.TCB_no);
	}
//	task->timeout=conn->timeout;
	return ret;
}

static void *thread_work(void *param)
{
resource *rs=(resource *)param;
int ret,fds;
TCB *task;
struct epoll_event event;

	rs->tid=pthread_self();
	while(1) {
	int timeout=0;
		if(!rs->tc.uc_stack.ss_size) {
			rs->tc.uc_stack.ss_sp=malloc(4096);
			rs->tc.uc_stack.ss_size=4096;
			swapcontext(&rs->tc,&rs->tc); //==setjmp();
		} else mthr_showid_del(rs->tid);
//从就绪队列取一个任务
		pthread_mutex_lock(&rpool.mut);
		while(!(task=rdy_get())) {
			if(rpool.flg >= tpool.rdy_num) break;
			rpool.flg++;
			ret=pthread_cond_wait(&rpool.cond,&rpool.mut); //没有任务，等待
 			rpool.flg--;
		}
		pthread_mutex_unlock(&rpool.mut);
		if(!task) {
			fds = epoll_wait(g_epoll_fd, &event, 1 , -1);
			if(fds < 0){
       	 			ShowLog(1,"%s:epoll_wait err=%d,%s",__FUNCTION__,errno,strerror(errno));
				sleep(30);
				continue;
       	 		}
		 	task = (TCB *)event.data.ptr;
			timeout=task->timeout;
			task->timeout=0;
			if(task->events == 0X10||task->fd==-1) {
				ShowLog(1,"%s:task already timeout",__FUNCTION__);
				continue;//已经进入超时状态
			}
			task->events=event.events;
		}
		rs->timestamp=now_usec();
		
		task->uc.uc_link=&rs->tc;
		if(task->uc.uc_stack.ss_size) {//fiber task
			set_showid(task->ctx);//Showid 应该在会话上下文结构里 
ShowLog(5,"%s:send to fiber",__FUNCTION__);
			setcontext(&task->uc);	//== longjmp()
			continue;//no action,logic only
		}
		ret=do_work(task); //进行你的服务
		task->timestamp=now_usec();//task与rs的差就是应用任务执行时间
		switch(ret) {
		case -1:
			do_epoll(task,EPOLL_CTL_DEL,0);
			client_del(task);
		case THREAD_ESCAPE:
			break;
		default:
			if(!task->timeout) task->timeout=timeout; //timeout 没有被应用设置过
			if(do_epoll(task,EPOLL_CTL_MOD,0) && errno != EEXIST) {
				ShowLog(1,"%s:cancel by server",__FUNCTION__);
				client_del(task);
			}
			break;
		}
		mthr_showid_del(rs->tid);
	}
	ShowLog(1,"%s:tid=%lX canceled",__FUNCTION__,pthread_self());
	mthr_showid_del(rs->tid);
	rs->timestamp=now_usec();
	rs->status=0;
	rs->tid=0;
	if(rs->tc.uc_stack.ss_sp) {
		free(rs->tc.uc_stack.ss_sp);
		rs->tc.uc_stack.ss_sp=NULL;
		rs->tc.uc_stack.ss_size=0;
	}
	return NULL;
}

ucontext_t * get_uc(int TCB_no)
{
	if(TCB_no<0 || client_q.max_client <= TCB_no) return NULL;
	return &client_q.pool[TCB_no].uc;
}

//检查超时的连接
int check_TCB_timeout()
{
int i,cltnum=client_q.max_client;
TCB * tp=client_q.pool;
INT64 now=now_usec();
int num=0,t;

        for(i=0;i<cltnum;i++,tp++) {
		if(tp->timeout<=0 || tp->fd<0) continue;
               	t=(int)((now-tp->timestamp)/1000000);
		if(t<tp->timeout) continue;
		tp->events=0X10;
                if(tp->call_back) TCB_add(NULL,tp->conn.TCB_no);
                else {
			do_epoll(tp,EPOLL_CTL_DEL,0);
			client_del(tp);
			if(tp->status<1) ShowLog(1,"%s:TCB:%d to cancel,t=%d",__FUNCTION__,i,t);
			else ShowLog(1,"%s:TCB:%d to delete,t=%d",__FUNCTION__,i,t);
			num++;
		}
        }
        return num;
}

static int get_task_no()
{
int i,ret;
struct timespec abstime;
	abstime.tv_sec=0;
	pthread_mutex_lock(&client_q.mut);
	while(0>(i=TCB_get(&client_q.free_q))) {
		if(abstime.tv_sec==0) ShowLog(1,"%s:超过最大连接数！",__FUNCTION__);
		clock_gettime(CLOCK_REALTIME, &abstime);
		abstime.tv_sec+=5;
		ret=pthread_cond_timedwait(&client_q.cond,&client_q.mut,&abstime);
		if(ret==ETIMEDOUT) {
			pthread_mutex_unlock(&client_q.mut);
			check_TCB_timeout();
			pthread_mutex_lock(&client_q.mut);
		}
		continue;
	}
	pthread_mutex_unlock(&client_q.mut);
	return i;
}

/***************************************************************
 * 线程池模型的入口函数，主线程主要负责资源管理
 *应用插件的服务函数在
 * extern srvfunc Function[];
 */
int TPOOL_srv(void (*conn_init)(T_Connect *,T_NetHead *),void (*quit)(int),void (*poolchk)(void),int sizeof_gda)
{
int ret,i;
int s;
struct sockaddr_in sin,cin;
struct servent *sp;
char *p;
struct timeval tm;
fd_set efds;
socklen_t leng=1;
int sock=-1;
srvfunc *fp;
TCB *task;
struct linger so_linger;

	signal(SIGPIPE,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	signal(SIGINT ,SIG_IGN);
	signal(SIGPWR ,quit);
	signal(SIGTERM,quit);

	p=getenv("SERVICE");
	if(!p || !*p) {
		ShowLog(1,"缺少环境变量 SERVICE ,不知守候哪个端口！");
		quit(3);
	}
//测试端口是否被占用 
	sock=tcpopen("localhost",p);
	if(sock>-1) {
		ShowLog(1,"端口 %s 已经被占用",p);
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
	leng=1;
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&leng,sizeof(leng));
//避免 TIME_WAIT
        so_linger.l_onoff=1;
        so_linger.l_linger=0;
        ret=setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof so_linger);
        if(ret) ShowLog(1,"set SO_LINGER err=%d,%s",errno,strerror(errno));

	listen(sock,client_q.max_client);

	ShowLog(0,"main start tid=%lX sock=%d",pthread_self(),sock);
	
	int repeat=0;
	leng=sizeof(cin);

	while(1) {
		do {
			FD_ZERO(&efds);
			FD_SET(sock, &efds);
//健康检查周期
			tm.tv_sec=15;
			tm.tv_usec=0;
			ret=select(sock+1,&efds,NULL,&efds,&tm);
			if(ret==-1) {
				ShowLog(1,"select error %s",strerror(errno));
				close(sock);
				quit(3);
			}
			if(ret==0) {
				check_TCB_timeout();
				if(poolchk) poolchk();
			}
		} while(ret<=0);
		i=get_task_no();
		task=&client_q.pool[i];
		s=accept(sock,(struct sockaddr *)&cin,&leng);
		if(s<0) {
			ShowLog(1,"%s:accept err=%d,%s",__FUNCTION__,errno,strerror(errno));
                       	client_del(task);
			switch(errno) {
			case EMFILE:	//fd用完了,其他线程还要继续工作，主线程休息一下。  
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
		task->fd=task->conn.Socket=s;
		task->timestamp=now_usec();
		task->timeout=60;
		task->status=-1;
		task->conn.only_do=(int (*)())conn_init;
		ret=do_epoll(task,EPOLL_CTL_ADD,0);//任务交给其他线程做
	}
	
	close(sock);
	tpool_free();
	return (0);
}

