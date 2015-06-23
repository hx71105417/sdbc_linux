/**************************************************
 * SDBC连接池管理
 * 这是一个按排队连接的程序，
 * 没有成功，归还连接还需要遍历数组     
 **************************************************/
#include <ctype.h>
#include <sys/utsname.h>
#include <bignum.h>
#include <pack.h>
#include <json_pack.h>
#include <sccli.h>
#include <sdbc.h>
#include "scpool.h"

#include "logs.tpl"
#include "logs.stu"
static int log_level=0;

T_PkgType SCPOOL_tpl[]={
        {CH_INT,sizeof(int),"d_node",0,-1},
        {CH_CHAR,17,"DEVID"},
        {CH_CHAR,256,"LABEL"},
        {CH_CHAR,17,"UID"},
        {CH_CHAR,14,"PWD"},
        {CH_INT,sizeof(int),"NUM"},
        {CH_INT,sizeof(int),"NEXT_d_node"},
        {CH_CHAR,81,"HOST"},
        {CH_CHAR,21,"PORT"},
        {CH_INT,sizeof(int),"MTU"},
        {CH_CHAR,172,"family"},
        {-1,0,0,0}
};

extern T_PkgType SCPOOL_tpl[];
typedef struct {
        int d_node;
        char DEVID[17];
        char LABEL[256];
        char UID[17];
        char PWD[14];
        int NUM;
        int NEXT_d_node;
        char HOST[81];
        char PORT[21];
        int MTU;
        char family[172];
} SCPOOL_stu;

//一个连接可以分配给 TCBNUM个任务，以实现异步流水线    
#define TCBNUM 3

typedef struct {
	int next;
	int tcb_num;
        int TCB_q[TCBNUM];
	T_Connect Conn;
	T_CLI_Var cli;
	INT64 timestamp;
	pthread_mutex_t mut;
} resource;

typedef struct {
	pthread_mutex_t mut;
	pthread_cond_t cond;
	int d_node;
	int resource_num;
	SCPOOL_stu log;
	u_int family[32];
	resource *lnk;
	int free_q;
}pool;

static int SCPOOLNUM=0;
static pool *scpool=NULL;
static void free_server(void *p)
{
}

//释放连接池  
void scpool_free()
{
int i,n;

	if(!scpool) return;
	for(n=0;n<SCPOOLNUM;n++) {
		pthread_cond_destroy(&scpool[n].cond);
		pthread_mutex_destroy(&scpool[n].mut);
		if(scpool[n].lnk) {
			for(i=0;i<scpool[n].resource_num;i++) {
			    if(scpool[n].lnk[i].Conn.Socket > -1) {
				disconnect(&scpool[n].lnk[i].Conn);
			    }
			     pthread_mutex_destroy(&scpool[n].lnk[i].mut);
			}
			free(scpool[n].lnk);
		}
	}
	free(scpool);
	scpool=NULL;
}

//初始化连接池  
int scpool_init()
{
int n,i,ret;
char *p,buf[512];
INT64 now;
FILE *fd;
JSON_OBJECT cfg,json;
SCPOOL_stu node;

	if(scpool) return 0;
	p=getenv("SCPOOLCFG");
	if(!p||!*p) {
		ShowLog(1,"%s:缺少环境变量SCPOOLCFG!",__FUNCTION__);
		return -1;
	}
	fd=fopen((const char *)p,"r");
	if(!fd) {
		ShowLog(1,"%s:CFGFILE %s open err=%d,%s",__FUNCTION__,
			p,errno,strerror(errno));
		return -2;
	}
	cfg=json_object_new_array();
	while(!ferror(fd)) {
		fgets(buf,sizeof(buf),fd);
		if(feof(fd)) break;
		TRIM(buf);
		if(!*buf || *buf=='#') continue;
		ret=net_dispack(&node,buf,SCPOOL_tpl);
		if(ret<=0) continue;
		json=json_object_new_object();
		struct_to_json(json,&node,SCPOOL_tpl,0);
		json_object_array_add(cfg,json);
	}
	fclose(fd);
	SCPOOLNUM=json_object_array_length(cfg);
	if(SCPOOLNUM <=0 ) {
		json_object_put(cfg);
		ShowLog(1,"%s:empty SCPOOL",__FUNCTION__);
		return -3;
	}
	scpool=(pool *)malloc(SCPOOLNUM * sizeof(pool));
	if(!scpool) {
		json_object_put(cfg);
		SCPOOLNUM=0;
		return MEMERR;
	}

	p=getenv("SCPOOL_LOGLEVEL");
	if(p && isdigit(*p)) log_level=atoi(p);

	now=now_usec();
    for(n=0;n<SCPOOLNUM;n++) {

	if(0!=(i=pthread_mutex_init(&scpool[n].mut,NULL))) {
		ShowLog(1,"%s:mutex_init err %s",__FUNCTION__,
			strerror(i));
		json_object_put(cfg);
		return -12;
	}
	
	if(0!=(i=pthread_cond_init(&scpool[n].cond,NULL))) {
		ShowLog(1,"%s:cond init  err %s",__FUNCTION__,
			strerror(i));
		json_object_put(cfg);
		return -13;
	}
	json=json_object_array_get_idx(cfg,n);
	json_to_struct(&scpool[n].log,json,SCPOOL_tpl);
	scpool[n].d_node=scpool[n].log.d_node;
	scpool[n].resource_num=scpool[n].log.NUM>0?scpool[n].log.NUM:1;
	scpool[n].lnk=(resource *)malloc(scpool[n].resource_num * sizeof(resource));
	if(!scpool[n].lnk) {
		ShowLog(1,"%s:malloc lnk error!",__FUNCTION__);
		scpool[n].resource_num=0;
		continue;
	}
	scpool[n].free_q=scpool[n].resource_num-1;
	for(i=0;i<scpool[n].resource_num;i++) {
	int j;
		pthread_mutex_init(&scpool[n].lnk[i].mut,NULL);
		Init_CLI_Var(&scpool[n].lnk[i].cli);
		scpool[n].lnk[i].cli.Errno=-1;
		initconnect(&scpool[n].lnk[i].Conn);
		strcpy(scpool[n].lnk[i].Conn.Host,scpool[n].log.HOST);
		strcpy(scpool[n].lnk[i].Conn.Service,scpool[n].log.PORT);
		scpool[n].lnk[i].Conn.timeout=-1;
		scpool[n].lnk[i].Conn.freevar=free_server;
		if(*scpool[n].log.family)
			str_a64n(32,scpool[n].log.family,scpool[n].family);
		scpool[n].lnk[i].tcb_num=0;
		if(i<scpool[n].resource_num-1) scpool[n].lnk[i].next=i+1;
                else scpool[n].lnk[i].next=0;
		 for(j=0;j<TCBNUM;j++) {
                       scpool[n].lnk[i].TCB_q[j]=-1;
                }
		scpool[n].lnk[i].timestamp=now;
	}
	ShowLog(2,"scpool[%d],link num=%d",n,scpool[n].resource_num);
    }
	json_object_put(cfg);
	return SCPOOLNUM;
}
inline int lnk_no(pool *pl,T_Connect *conn)
{
T_CLI_Var *clip=(T_CLI_Var *)conn->Var;
	
	if(!clip) {
		ShowLog(1,"%s:clip empty!",__FUNCTION__);
		return -1;
	}
	if(conn != &pl->lnk[clip->NativeError].Conn) {
		ShowLog(1,"%s:NativeError=%d",__FUNCTION__,
			clip->NativeError);
		return -1;
	}
	return clip->NativeError;
}

inline int get_lnk_no(pool *pl)
{
int i,*ip,*np;
resource *rs;

        if(pl->free_q<0) return -1;
	ip=&pl->free_q;
	rs=&pl->lnk[*ip];
        i=rs->next;
	np=&pl->lnk[i].next;
        if(i==*ip) *ip=-1;
        else rs->next=*np;
        *np=-1;
        return i;
}

/*
inline void add_lnk(pool *pl,int i)
{
        if(pl->lnk[i].next>=0) return;
        if(pl->free_q < 0) {
                pl->free_q=i;
                pl->lnk[i].next=i;
        } else { //插入队头  
        resource *rs=&pl->lnk[pl->free_q];
                pl->lnk[i].next=rs->next;
                rs->next=i;
		if(rs->cli.Errno<0) pl->lnk[pl->free_q]=i;//坏连接排队尾 
        }
}
*/

inline void add_lnk(pool *pl,int i)
{
resource *rsi = &pl->lnk[i];
int *np,*ip=&rsi->next;
        if(*ip>=0) return;
	np=&pl->free_q;
        if(*np < 0) {
                *np=i;
                *ip=i;
	} else { //插入队头  
	resource *rs=&pl->lnk[*np];
                *ip=rs->next;
                rs->next=i;
/*
		if(*rsi->TCB_q>=0 || rsi->cli.Errno<0) {
			*np=i;//坏,忙连接排队尾
ShowLog(5,"%s:lnk=%d,TCB[0]=%d,Errno=%d,插入队尾",__FUNCTION__,i,*rsi->TCB_q,rsi->cli.Errno);
		}
*/
        }
}
static int log_ret(T_Connect *conn,T_NetHead *head)
{
int ret,i;
T_SRV_Var *srvp=(T_SRV_Var *)conn->Var;
resource *rs;
pool *pl;
T_Connect *Conn;
sdbcfunc f;
log_stu logs;

	clr_event(srvp->TCB_no);
//ShowLog(5,"%s:tid=%lu,poolno=%d,entry TCB:%d",__FUNCTION__,pthread_self(),srvp->poolno,srvp->TCB_no);
	if(srvp->poolno < 0) {
		ShowLog(1,"%s:tid=%lu,poolno=%d,TCB:%d",__FUNCTION__,
			pthread_self(),srvp->poolno,srvp->TCB_no);
		unbind_sc(srvp->TCB_no);
		return -1;
	}
	pl=&scpool[srvp->poolno];
	rs=pl->lnk;
	for(ret=0;ret<pl->resource_num;ret++,rs++) {
		for(i=0;i<rs->tcb_num;i++)
			if(rs->TCB_q[i] == srvp->TCB_no) break;
		if(i<rs->tcb_num) break;
	}
	if(ret == pl->resource_num) {
		ShowLog(1,"%s:没找到连接",__FUNCTION__);
		unbind_sc(srvp->TCB_no);
		return -1;
	}
	f=rs->Conn.only_do;
	set_callback(srvp->TCB_no,rs->Conn.only_do);
	rs->Conn.only_do=NULL;
ShowLog(5,"%s:TCB=%d,poolno=%d.%d",__FUNCTION__,srvp->TCB_no,srvp->poolno,ret);
	Conn=&rs->Conn;
	ret=get_event_status(srvp->TCB_no);
        if(ret!=1) { //EPOLLIN
                ShowLog(1,"%s:呼叫服务器返回超时",__FUNCTION__);
		unbind_sc(srvp->TCB_no);
		rs->cli.Errno=-1;
                release_SC_connect(&Conn,srvp->TCB_no,srvp->poolno);
		TCB_add(NULL,srvp->TCB_no); //加入到主任务队列
		return -5;
        }

	ret=RecvPack(&rs->Conn,head);
	if(ret) { //网络失败
		rs->cli.Errno=errno;
		stptok(strerror(errno),rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		unbind_sc(srvp->TCB_no);
		ShowLog(1,"%s:network error %d,%s",__FUNCTION__,rs->cli.Errno,rs->cli.ErrMsg);
		rs->cli.Errno=-1;
		release_SC_connect(&Conn,srvp->TCB_no,srvp->poolno);
		TCB_add(NULL,srvp->TCB_no); //加入到主任务队列
		return -5;
	}
	if(head->ERRNO1 || head->ERRNO2) {  //login失败
		ShowLog(1,"%s:login error ERRNO1=%d,ERRNO2=%d,%s",__FUNCTION__,
			head->ERRNO1,head->ERRNO2,head->data);
errret:
		unbind_sc(srvp->TCB_no);
		stptok(head->data,rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		rs->cli.Errno=-1;
		release_SC_connect(&Conn,srvp->TCB_no,srvp->poolno);
		TCB_add(NULL,srvp->TCB_no); //加入到主任务队列
		return -5;
	}
	net_dispack(&logs,head->data,log_tpl);
	strcpy(rs->cli.DBOWN,logs.DBOWN);
	strcpy(rs->cli.UID,logs.DBUSER);
//取服务名
	ret=init_svc_no(&rs->Conn);
	if(ret) { //取服务名失败
		ShowLog(1,"%s:init_svc_no error ERRNO1=%d,ERRNO2=%d,%s",__FUNCTION__,
			head->ERRNO1,head->ERRNO2,head->data);
		goto errret;
	}
	rs->cli.Errno=ret;
ShowLog(5,"%s:TCB=%d,init_svc_no Errno=%d",__FUNCTION__,srvp->TCB_no,rs->cli.Errno);
	*rs->cli.ErrMsg=0;
	return f(conn,head);
}

static int to_log_ret(T_Connect *conn,T_NetHead *head)
{
int ret,i;
T_SRV_Var *srvp=(T_SRV_Var *)conn->Var;
resource *rs;
pool *pl;

	unset_callback(srvp->TCB_no);
	if(srvp->poolno<0) {
		ShowLog(1,"%s:tid=%lu,poolno=%d,entry TCB:%d",__FUNCTION__,
			pthread_self(),srvp->poolno,srvp->TCB_no);
		unbind_sc(srvp->TCB_no);
		return -1;
	}
	pl=&scpool[srvp->poolno];
	rs=pl->lnk;
	for(ret=0;ret<pl->resource_num;ret++,rs++) {
		for(i=0;i<rs->tcb_num;i++)
                        if(rs->TCB_q[i] == srvp->TCB_no) break;
                if(i<rs->tcb_num) break;

	}
	if(ret == pl->resource_num) {
		unbind_sc(srvp->TCB_no);
		return -1;
	}
ShowLog(5,"%s:TCB:%d,call_back=%lu,poolno=%d.%d",__FUNCTION__,srvp->TCB_no,log_ret,srvp->poolno,ret);
//任务将回到epoll队列 
	set_event(srvp->TCB_no,rs->Conn.Socket,log_ret,60);
	return -5;
}

//连接
static int sc_connect(pool *p1,resource *rs)
{
int ret=-1;
T_NetHead Head;
struct utsname ubuf;
char buf[200],*p,addr[20];

	rs->Conn.timeout=-1;
	ret=Net_Connect(&rs->Conn,&rs->cli,*p1->log.family?p1->family:NULL);
	if(ret) {
		rs->cli.Errno=errno;
		stptok(strerror(errno),rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		return -1;
	}
	rs->Conn.freevar=free_server;

//login
	uname(&ubuf);
	p=buf;
	Head.O_NODE=LocalAddr(rs->Conn.Socket,addr);
	p+=sprintf(p,"%s|%s|%s,%s|||",p1->log.DEVID,p1->log.LABEL,
		ubuf.nodename,addr);
	rs->Conn.MTU=p1->log.MTU;
	Head.PROTO_NUM=0;
	Head.D_NODE=p1->log.NEXT_d_node;
	Head.ERRNO1=rs->Conn.MTU;
	Head.ERRNO2=Head.PKG_REC_NUM=0;
	Head.data=buf;
	Head.PKG_LEN=strlen(Head.data);
	ret=SendPack(&rs->Conn,&Head);
//任务将回到rdy队列 
	rs->Conn.only_do=set_callback(rs->TCB_q[0],to_log_ret);
	if(!rs->Conn.only_do) {
		rs->Conn.only_do=(sdbcfunc)1;
	}
ShowLog(5,"%s:tid=%lu,to_log_ret=%lu,only_do=%lu,TCB:%d",__FUNCTION__,
		pthread_self(),to_log_ret,rs->Conn.only_do,rs->TCB_q[0]);
	return 0;
}

//取连接  
T_Connect * get_SC_connect(int TCBno,int n,int flg)
{
int i,ret;
pool *pl;
resource *rs;
pthread_t tid=pthread_self();

	if(!scpool || n<0 || n>=SCPOOLNUM) return NULL;
	pl=&scpool[n];
	if(!pl->lnk) {
		ShowLog(1,"%s:无效的连接池[%d]",__FUNCTION__,n);
		return NULL;
	}
	if(0!=pthread_mutex_lock(&pl->mut)) return (T_Connect *)-1;
    while(1) {
	i=get_lnk_no(pl);
	if(i>=0) {
		rs=&pl->lnk[i];
ShowLog(5,"%s:i=%d,TCB:%d,tid=%lu,tcb_num=%d",__FUNCTION__,i,TCBno,tid,rs->tcb_num);
		rs->TCB_q[rs->tcb_num++]=TCBno;
		rs->timestamp=now_usec();
		if(rs->Conn.Socket<0 || rs->cli.Errno<0) {
			pthread_mutex_unlock(&pl->mut);
			ret=sc_connect(pl,rs);
			pthread_mutex_lock(&pl->mut);
			switch(ret) {
			case -1:
				ShowLog(1,"%s:scpool[%d].%d 连接%s/%s错:err=%d,%s",
					__FUNCTION__,n,i,pl->log.HOST,pl->log.PORT,
					rs->cli.Errno, rs->cli.ErrMsg);
				--rs->tcb_num;
				rs->TCB_q[rs->tcb_num]=-1;
				add_lnk(pl,i);
				pthread_mutex_unlock(&pl->mut);
				return (T_Connect *)-1;
			case 0:
				break;
			default:
				rs->cli.Errno=-1;
				--rs->tcb_num;
				rs->TCB_q[rs->tcb_num]=-1;
				ShowLog(1,"%s:scpool[%d].%d 连接%s/%s错:err=%d,%s",
					__FUNCTION__,n,i,pl->log.HOST,pl->log.PORT,
					rs->cli.Errno, rs->cli.ErrMsg);
				add_lnk(pl,i);
				continue;
			} 
		} 
		pthread_mutex_unlock(&pl->mut);
		pthread_mutex_lock(&rs->mut);
		if(log_level) ShowLog(log_level,"%s tid=%lu,TCB:%d,pool[%d].lnk[%d]",__FUNCTION__,
			tid,TCBno,n,i);
		rs->cli.Errno=0;
		rs->cli.NativeError=i;
		*rs->cli.ErrMsg=0;
		return &rs->Conn;
	}
	if(flg) {	//flg !=0,don't wait
		pthread_mutex_unlock(&pl->mut);
		return NULL;
	}
	if(log_level) ShowLog(log_level,"%s tid=%lu pool[%d] TCB:%d,suspend",__FUNCTION__,pthread_self(),n,TCBno);
	pthread_cond_wait(&pl->cond,&pl->mut); //没有资源，等待 
	if(log_level) ShowLog(log_level,"%s tid=%lu pool[%d] TCB:%d,weakup",__FUNCTION__,pthread_self(),n,TCBno);
    }
}
int share_lnk(int poolno,T_Connect *conn)
{
resource *rs;
T_CLI_Var *clip;
pool *pl;
	if(!conn || !scpool || poolno<0 || poolno>=SCPOOLNUM) {
		return -1;
	}
	clip=(T_CLI_Var *)conn->Var;
	if(!clip) {
		ShowLog(1,"%s:clip empty!",__FUNCTION__);
		return -2;
	}
	pl=&scpool[poolno];
	rs=&pl->lnk[clip->NativeError];
	if(&rs->Conn != conn) {
		ShowLog(1,"%s:NativeError=%d",__FUNCTION__,clip->NativeError);
		return -3;
	}
	pthread_mutex_unlock(&rs->mut);
	if(!rs->cli.Errno && rs->tcb_num < TCBNUM-1) {
		pthread_mutex_lock(&pl->mut);
		add_lnk(pl,clip->NativeError);
		pthread_mutex_unlock(&pl->mut);
		pthread_cond_signal(&pl->cond); //如果有等待连接的线程就唤醒它 
		return 0;
        }
	return 1;
}

//归还数据库连接  
void release_SC_connect(T_Connect **Connect,int TCBno,int n)
{
int ret,i,j,flg;
pthread_t tid=pthread_self();
pool *pl;
resource *rs;
T_CLI_Var *clip;
//ShowLog(5,"%s:tid=%lu,TCB:%d,poolno=%d",__FUNCTION__,pthread_self(),TCBno,n);
	if(!Connect || !scpool || n<0 || n>=SCPOOLNUM) {
		ShowLog(1,"%s:TCB:%d,poolno=%d,错误的参数",__FUNCTION__,TCBno,n);
		return;
	}
	if(!*Connect) {
		ShowLog(1,"%s:TCB:%d,Conn is Empty!",__FUNCTION__,TCBno);
		return;
	}
	clip=(T_CLI_Var *)((*Connect)->Var);
	pl=&scpool[n];
	if(0!=pthread_mutex_lock(&pl->mut)) {
		ShowLog(1,"%s:pool[%d],TCB:%d,mutex error=%d,%s",
			__FUNCTION__,n,TCBno,errno,strerror(errno));
		return;
	}
	i=lnk_no(pl,*Connect);
	if(i>=0) {
		rs=&pl->lnk[i];
		pthread_mutex_unlock(&rs->mut);
if(clip != &rs->cli) {
	ShowLog(1,"%s:TCB:%d,clip not equal!",__FUNCTION__,TCBno);
}
		flg=0;
		if(rs->cli.Errno==-1) {  //连接失效
			ShowLog(1,"%s:scpool[%d].%d to fail!",__FUNCTION__,n,i);
			disconnect(&rs->Conn);
			for(j=0;j<TCBNUM;j++) {
				if(rs->TCB_q[j] >= 0 && rs->TCB_q[j]!=TCBno) {
//释放所有使用这个连接的任务
					unbind_sc(rs->TCB_q[j]);
	       	                        TCB_add(NULL,rs->TCB_q[j]);
	       	                }
	       	                rs->TCB_q[j]=-1;
	                }
	                flg=1;
		}
		if(!flg) {
                    for(j=0;j<TCBNUM;j++) {
                        if(rs->TCB_q[j] == TCBno){
                                flg=1;
                        }
                        if(flg) {
                            if(j<TCBNUM-1)
                                rs->TCB_q[j]=rs->TCB_q[j+1];
                            else rs->TCB_q[j]=-1;
                        }
                    }
                    if(flg) rs->tcb_num--;
                } else rs->tcb_num=0;
ShowLog(5,"%s:tid=%lu,TCB_q[0]=%d,Errno=%d",__FUNCTION__,pthread_self(),rs->TCB_q[0],rs->cli.Errno);
		add_lnk(pl,i);
		if(rs->TCB_q[0] >= 0) {//激活下一个任务
        	sdbcfunc f;
			f=get_callback(rs->TCB_q[0]);
			ret=get_event_status(rs->TCB_q[0]);
			if(!ret && f) {
				ret=set_event(rs->TCB_q[0],rs->Conn.Socket,f,60);
				ShowLog(5,"%s:激活:%d,ret=%d",__FUNCTION__,rs->TCB_q[0],ret);
			}
		}

		pthread_mutex_unlock(&pl->mut);
		pthread_cond_signal(&pl->cond); //如果有等待连接的线程就唤醒它 
  		rs->timestamp=now_usec();
		clip->Errno=0;
		*clip->ErrMsg=0;
		*Connect=NULL;
		if(log_level) ShowLog(log_level,"%s tid=%lu,TCB:%d,pool[%d].lnk[%d]",__FUNCTION__,
				tid,TCBno,n,i);
		return;
	}
	pthread_mutex_unlock(&pl->mut);
ShowLog(1,"%s:在pool[%d]中未发现该TCBno:%d",__FUNCTION__,n,TCBno);
	clip->Errno=0;
	*clip->ErrMsg=0;
	*Connect=NULL;
}
//连接池监控 
void scpool_check()
{
int n,i,j,num;
pool *pl;
resource *rs;
INT64 now;
char buf[32];
T_Connect *conn=NULL;

	if(!scpool) return;
	now=now_usec();
	pl=scpool;

	for(n=0;n<SCPOOLNUM;n++,pl++) {
		if(!pl->lnk) continue;
		rs=pl->lnk;
		num=pl->resource_num;
//		if(log_level) ShowLog(log_level,"%s:scpool[%d],num=%d",__FUNCTION__,n,num);
		pthread_mutex_lock(&pl->mut);
		for(i=0;i<num;i++,rs++) if(rs->tcb_num==0) {
			if(rs->Conn.Socket>-1 && (now-rs->timestamp)>299000000) {
//空闲时间太长了     
				disconnect(&rs->Conn);
				rs->cli.Errno=-1;
				if(log_level)
					ShowLog(log_level,"%s:Close SCpool[%d].lnk[%d],since %s",__FUNCTION__,
					n,i,rusecstrfmt(buf,rs->timestamp,YEAR_TO_USEC));
			}
		} else {
			if(rs->Conn.Socket>-1 && (now-rs->timestamp)>299000000) {
//占用时间太长了     
			   for(j=0;j<rs->tcb_num;j++) {
				if(-1==get_TCB_status(rs->TCB_q[j])) {
				//TCB已经结束，释放之
					if(log_level) ShowLog(log_level,"%s:scpool[%d].lnk[%d] TCB:%d to be release",
						__FUNCTION__,n,i,rs->TCB_q[j]);
					rs->cli.Errno=-1;
					conn=&rs->Conn;
					release_SC_connect(&conn,rs->TCB_q[j],n);
				} else {
				    if(log_level) ShowLog(log_level,"%s:scpool[%d].lnk[%d] used by TCB:%d,since %s",
					__FUNCTION__,n,i,rs->TCB_q[j],
					rusecstrfmt(buf,rs->timestamp,YEAR_TO_USEC));
				}
			   }
			}
		}
		pthread_mutex_unlock(&pl->mut);
	}
}
/**
 * 根据d_node取连接池号  
 * 失败返回-1
 */
int get_scpool_no(int d_node)
{
int n;
	if(!scpool) return -1;
	for(n=0;n<SCPOOLNUM;n++) {
		if(scpool[n].d_node==d_node) return n;
	}
	return -1;
}

int get_scpoolnum()
{
	return SCPOOLNUM;
}

char *get_LABEL(poolno)
{
	if(poolno<0 || poolno>=SCPOOLNUM) return NULL;
	return scpool[poolno].log.LABEL;
}

int  conn_lock(int poolno,int conn_no)
{
pool *pl;
resource *rs;

        if(poolno<0 || poolno>=SCPOOLNUM) return -1;
        pl=&scpool[poolno];
        if(conn_no < 0 || conn_no >= pl->resource_num)
                return -1;
        rs=&pl->lnk[conn_no];
        return pthread_mutex_lock(&rs->mut);
}

int  conn_unlock(int poolno,int conn_no)
{
pool *pl;
resource *rs;

        if(poolno<0 || poolno>=SCPOOLNUM) return -1;
        pl=&scpool[poolno];
        if(conn_no < 0 || conn_no >= pl->resource_num)
                return -1;
        rs=&pl->lnk[conn_no];
        return pthread_mutex_unlock(&rs->mut);
}

