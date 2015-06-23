/**************************************************
 * SDBC连接池管理
 **************************************************/
#include <ctype.h>
#include <sys/utsname.h>
#include <bignum.h>
#include <pack.h>
#include <json_pack.h>
#include <sccli.h>
#include <scsrv.h>
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

//连接
static int sc_connect(pool *p1,resource *rs)
{
int ret=-1;
T_NetHead Head;
struct utsname ubuf;
char buf[200],*p,addr[20];
log_stu logs;

	rs->Conn.timeout=-1;
	ret=Net_Connect(&rs->Conn,&rs->cli,*p1->log.family?p1->family:NULL);
	if(ret) {
		rs->cli.Errno=errno;
		stptok(strerror(errno),rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		return -1;
	}
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
	ret=RecvPack(&rs->Conn,&Head);
	if(ret) {
		rs->cli.Errno=errno;
		stptok(strerror(errno),rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		disconnect(&rs->Conn);
		ShowLog(1,"%s:network error %d,%s",__FUNCTION__,rs->cli.Errno,rs->cli.ErrMsg);
		rs->cli.Errno=-1;
		return -2;
	}
	if(Head.ERRNO1 || Head.ERRNO2) {
		ShowLog(1,"%s:login error ERRNO1=%d,ERRNO2=%d,%s",__FUNCTION__,
			Head.ERRNO1,Head.ERRNO2,Head.data);
		disconnect(&rs->Conn);
		stptok(Head.data,rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		rs->cli.Errno=-1;
		return -3;
	}
	net_dispack(&logs,Head.data,log_tpl);
	strcpy(rs->cli.DBOWN,logs.DBOWN);
	strcpy(rs->cli.UID,logs.DBUSER);
//取服务名
	ret=init_svc_no(&rs->Conn);
	rs->cli.Errno=ret;
	*rs->cli.ErrMsg=0;
	return ret;
}

static int get_lnk_no(pool *pl)
{
int i;

	if(pl->free_q<0) return -1;
	i=pl->lnk[pl->free_q].next;
	if(i==pl->free_q) pl->free_q=-1;
	else pl->lnk[pl->free_q].next=pl->lnk[i].next;
	pl->lnk[i].next=-1;
	return i;
}
static void add_lnk(pool *pl,int i)
{
	if(pl->lnk[i].next>=0) return;
	if(pl->free_q < 0) {
		pl->free_q=i;
		pl->lnk[i].next=i;
	} else {
		pl->lnk[i].next=pl.lnk[pl->free_q].next;
		pl->lnk[pl->free_q].next=i;
		pl->free_q=i;
	}
}
//取连接  
T_Connect * get_SC_connect(int TCBno,int n,int flg)
{
int i,num;
pool *pl;
resource *rs;
pthread_t tid=pthread_self();

	if(!scpool || n<0 || n>=SCPOOLNUM) return NULL;
	pl=&scpool[n];
	if(!pl->lnk) {
		ShowLog(1,"%s:无效的连接池[%d]",__FUNCTION__,n);
		return (T_Connect *)-1;
	}
	num=pl->resource_num;
	if(0!=pthread_mutex_lock(&pl->mut)) return (T_Connect *)-1;
    while(1) {
	i=get_lnk_no(pl);
	if(i>=0) {
		rs=&pl->lnk[i];
		if(rs->Conn.Socket<0 || rs->cli.Errno<0) {
			switch(sc_connect(pl,rs)) {
			case -1:
			case -2:
			case -3:
				ShowLog(1,"%s:scpool[%d].%d 连接%s/%s错:err=%d,%s",
					__FUNCTION__,n,i,pl->log.HOST,pl->log.PORT,
					rs->cli.Errno, rs->cli.ErrMsg); 
				add_lnk(pl,i);
				pthread_mutex_unlock(&pl->mut);
				return (T_Connect *)-1;
			case 0:
				break;
			default:
				rs->cli.Errno=-1;
				ShowLog(1,"%s:scpool[%d].%d 连接%s/%s错:err=%d,%s",
					__FUNCTION__,n,i,pl->log.HOST,pl->log.PORT,
					rs->cli.Errno, rs->cli.ErrMsg);
				add_lnk(pl,i);
				continue;
			} 
		} 
		rs->TCB_q[rs->tcb_num++]=TCBno;
		rs->timestamp=now_usec();
		if(rs->tcb_num < TCBNUM) {
			add_lnk(pl,i);
			pthread_cond_signal(&pl->cond); //如果有等待连接的线程就唤醒它 
		}
		pthread_mutex_unlock(&pl->mut);
		if(log_level) ShowLog(log_level,"%s tid=%lu,TCB:%d,pool[%d].lnk[%d]",__FUNCTION__,
			tid,TCBno,n,i);
		rs->cli.Errno=0;
		*rs->cli.ErrMsg=0;
		return &rs->Conn;
	}
	if(flg) {	//flg !=0,don't wait
		pthread_mutex_unlock(&pl->mut);
		return NULL;
	}
	if(log_level) ShowLog(log_level,"%s tid=%lu pool[%d] suspend",__FUNCTION__,pthread_self(),n);
	pthread_cond_wait(&pl->cond,&pl->mut); //没有资源，等待 
	if(log_level) ShowLog(log_level,"%s tid=%lu pool[%d] weakup",__FUNCTION__,pthread_self(),n);
    }
}
//归还数据库连接  
void release_SC_connect(T_Connect **Connect,int TCBno,int n)
{
int i,j,flg,num;
pthread_t tid=pthread_self();
pool *pl;
resource *rs;
T_CLI_Var *clip;
//ShowLog(5,"%s:TCB:%d,poolno=%d",__FUNCTION__,TCBno,n);
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
	rs=pl->lnk;
	if(!rs) {
		ShowLog(1,"%s:无效的连接池[%d]",__FUNCTION__,n);
		return;
	}
	num=pl->resource_num;
	for(i=0;i<num;i++,rs++) if(*Connect == &rs->Conn) {
		if(0!=pthread_mutex_lock(&pl->mut)) {
			ShowLog(1,"%s:pool[%d].%d,TCB:%d,mutex error=%d,%s",
				__FUNCTION__,n,i,TCBno,errno,strerror(errno));
			return;
		}
if(clip != &rs->cli) {
	ShowLog(1,"%s:TCB:%d,clip not equal!",__FUNCTION__,TCBno);
}
		flg=0;
		if(rs->cli.Errno==-1) {  //连接失效
ShowLog(1,"%s:scpool[%d].%d fail!",__FUNCTION__,n,i);
			disconnect(&rs->Conn);
			for(j=0;j<TCBNUM;j++) {
				if(rs->TCB_q[j] >= 0 && rs->TCB_q[j]!=TCB_no) {
//释放所有使用这个连接的任务 
					unbind_sc(rs->TCB_q[j]);
					TCB_add(NULL,rs->tcb_q[j]);
				}
				rs->TCB_q[j]=-1;
			}
			flg=1;
		}
		add_lnk(pl,i);
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
ShowLog(1,"%s:在pool[%d]中未发现该TCBno:%d",__FUNCTION__,n,TCBno);
	clip->Errno=0;
	*clip->ErrMsg=0;
	*Connect=NULL;
}
//连接池监控 
void scpool_check()
{
int n,i,num;
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
		if(log_level) ShowLog(log_level,"%s:scpool[%d],num=%d",__FUNCTION__,n,num);
		pthread_mutex_lock(&pl->mut);
		for(i=0;i<num;i++,rs++) if(rs->TCBno<0) {
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
				if(-1==get_TCB_status(rs->TCBno)) {
				//TCB已经结束，释放之
					if(log_level) ShowLog(log_level,"%s:scpool[%d].lnk[%d] TCB:%d to be release",
						__FUNCTION__,n,i,rs->TCBno);
					rs->cli.Errno=-1;
					conn=&rs->Conn;
					release_SC_connect(&conn,rs->TCBno,n);
				} else {
				    if(log_level) ShowLog(log_level,"%s:scpool[%d].lnk[%d] used by TCB:%d,since %s",
					__FUNCTION__,n,i,rs->TCBno,
					rusecstrfmt(buf,rs->timestamp,YEAR_TO_USEC));
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

int conn_no(int poolno,T_Connect *conn)
{
int i,num;
pool *pl;
resource *rs;

	if(poolno<0 || poolno>=SCPOOLNUM) return -1;
	pl=&scpool[poolno];
	rs=pl->lnk;
	num=pl->resource_num;
	for(i=0;i<num;i++,rs++) if(conn == &rs->Conn) 
		return i;
	return -1;
}

int  conn_lock(int poolno,int conn_no)
{
int i,num;
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
int i,num;
pool *pl;
resource *rs;
	
	if(poolno<0 || poolno>=SCPOOLNUM) return -1;
	pl=&scpool[poolno];
	if(conn_no < 0 || conn_no >= pl->resource_num)
		return -1;
	rs=&pl->lnk[conn_no];
	return pthread_mutex_unlock(&rs->mut);
}

