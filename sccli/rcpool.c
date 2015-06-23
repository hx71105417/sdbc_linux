/**************************************************
 * rcpool:remote ca 
 * SDBC远程认证连接池管理 用于多线程客户端
 **************************************************/
#include <ctype.h>
#include <sys/utsname.h>
#include <bignum.h>
#include <pack.h>
#include <json_pack.h>
#include <scsrv.h>
#include <sccli.h>
#include "rcpool.h"
#include <scry.h>

#include <logs.tpl>
#include <logs.stu>
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

typedef struct {
	int next;
	T_Connect Conn;
	T_CLI_Var cli;
	INT64 timestamp;
} resource;

typedef struct {
	pthread_mutex_t mut;
	pthread_cond_t cond;
	int d_node;
	int resource_num;
	SCPOOL_stu log;
	u_int family[32];
	svc_table svc_tbl;
	resource *lnk;
	int free_q;
}pool;

static int RCPOOLNUM=0;
static pool *rcpool=NULL;
//释放连接池  
void rcpool_free()
{
int i,n;
T_NetHead Head;
	if(!rcpool) return;
	for(n=0;n<RCPOOLNUM;n++) {
		pthread_cond_destroy(&rcpool[n].cond);
		pthread_mutex_destroy(&rcpool[n].mut);
		if(rcpool[n].lnk) {
			for(i=0;i<rcpool[n].resource_num;i++) {
			    if(rcpool[n].lnk[i].Conn.Socket > -1) {
				Head.PROTO_NUM=get_srv_no(&rcpool[n].lnk[i].cli,"ctx_logout_svc");
				if(Head.PROTO_NUM > 1) { // logout
					Head.ERRNO1=0;
					Head.ERRNO2=PACK_NOANSER;
					Head.O_NODE=rcpool[n].lnk[i].cli.ctx_id;
					Head.data=NULL;
					Head.PKG_LEN=0;
					SendPack(&rcpool[n].lnk[i].Conn,&Head);
				}
				disconnect(&rcpool[n].lnk[i].Conn);
			    }
			}
			free(rcpool[n].lnk);
		}
	}
	free(rcpool);
	rcpool=NULL;
}

//初始化连接池  
int rcpool_init()
{
int n,i,ret;
char *p,buf[512];
INT64 now;
FILE *fd;
JSON_OBJECT cfg,json;
SCPOOL_stu node;

	if(rcpool) return 0;
	p=getenv("RCPOOLCFG");
	if(!p||!*p) {
		ShowLog(1,"%s:缺少环境变量RCPOOLCFG!",__FUNCTION__);
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
	RCPOOLNUM=json_object_array_length(cfg);
	if(RCPOOLNUM <=0 ) {
		json_object_put(cfg);
		ShowLog(1,"%s:empty RCPOOL",__FUNCTION__);
		return -3;
	}
	rcpool=(pool *)malloc(RCPOOLNUM * sizeof(pool));
	if(!rcpool) {
		json_object_put(cfg);
		RCPOOLNUM=0;
		return MEMERR;
	}

	p=getenv("SCPOOL_LOGLEVEL");
	if(p && isdigit(*p)) log_level=atoi(p);

	now=now_usec();
    for(n=0;n<RCPOOLNUM;n++) {

	if(0!=(i=pthread_mutex_init(&rcpool[n].mut,NULL))) {
		ShowLog(1,"%s:mutex_init err %s",__FUNCTION__,
			strerror(i));
		json_object_put(cfg);
		return -12;
	}
	
	if(0!=(i=pthread_cond_init(&rcpool[n].cond,NULL))) {
		ShowLog(1,"%s:cond init  err %s",__FUNCTION__,
			strerror(i));
		json_object_put(cfg);
		return -13;
	}
	json=json_object_array_get_idx(cfg,n);
	json_to_struct(&rcpool[n].log,json,SCPOOL_tpl);
	rcpool[n].d_node=rcpool[n].log.d_node;
	rcpool[n].svc_tbl.srvn=0;
	rcpool[n].resource_num=rcpool[n].log.NUM>0?rcpool[n].log.NUM:1;
	rcpool[n].lnk=(resource *)malloc(rcpool[n].resource_num * sizeof(resource));
	if(!rcpool[n].lnk) {
		ShowLog(1,"%s:malloc lnk error!",__FUNCTION__);
		rcpool[n].resource_num=0;
		continue;
	}
	rcpool[n].free_q=rcpool[n].resource_num-1;
	for(i=0;i<rcpool[n].resource_num;i++) {
		Init_CLI_Var(&rcpool[n].lnk[i].cli);
		rcpool[n].lnk[i].cli.Errno=-1;
		rcpool[n].lnk[i].cli.ctx_id=0;
		initconnect(&rcpool[n].lnk[i].Conn);
		rcpool[n].lnk[i].Conn.pos=i;
		strcpy(rcpool[n].lnk[i].Conn.Host,rcpool[n].log.HOST);
		strcpy(rcpool[n].lnk[i].Conn.Service,rcpool[n].log.PORT);
		if(*rcpool[n].log.family)
			str_a64n(32,rcpool[n].log.family,rcpool[n].family);
		if(i<rcpool[n].resource_num-1) rcpool[n].lnk[i].next=i+1;
                else rcpool[n].lnk[i].next=0;
		rcpool[n].lnk[i].timestamp=now;
	}
	ShowLog(2,"%s:rcpool[%d],link num=%d",__FUNCTION__,n,rcpool[n].resource_num);
    }
	json_object_put(cfg);
	return RCPOOLNUM;
}
static int lnk_no(pool *pl,T_Connect *conn)
{
int i,e;
resource *rs=pl->lnk;

       	if(conn->pos<pl->resource_num && conn != &pl->lnk[conn->pos].Conn) {
          	ShowLog(1,"%s:conn not equal pos=%d",__FUNCTION__, conn->pos);
       	} else return conn->pos;
	e=pl->resource_num;
	for(i=0;i<e;i++,rs++) {
		if(conn == &rs->Conn) {
			conn->pos=i;
			return i;
		}
	}
	return -1;
}

static int get_lnk_no(pool *pl)
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

static void add_lnk(pool *pl,int i)
{
int *np,*ip=&pl->lnk[i].next;
        if(*ip>=0) return;
	np=&pl->free_q;
        if(*np < 0) {
                *np=i;
                *ip=i;
	} else { //插入队头  
	resource *rs=&pl->lnk[*np];
                *ip=rs->next;
                rs->next=i;
		if(pl->lnk[i].Conn.Socket<0) *np=i;//坏连接排队尾 
        }
}
//连接
static int sc_connect(pool *p1,resource *rs)
{
int ret=-1;
T_NetHead Head;
struct utsname ubuf;
char buf[270],finger[256],*p,addr[20];
log_stu logs;
int timeout;

	ret=Net_Connect(&rs->Conn,&rs->cli,*p1->log.family?p1->family:NULL);
	if(ret) {
		rs->cli.Errno=errno;
		stptok(strerror(errno),rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		return -1;
	}

	p=getenv("TCPTIMEOUT");//服务器一侧的超时
	if(p && isdigit(*p)) {
		rs->Conn.timeout=timeout=60*atoi(p);
	} else timeout=rs->Conn.timeout=0;
//login
	uname(&ubuf);
	fingerprint(finger);

again_login:
	p=buf;
	p+=sprintf(p,"%s|%s|%s,%s|||",p1->log.DEVID,p1->log.LABEL,
		ubuf.nodename,finger);
	rs->Conn.MTU=p1->log.MTU;
	Head.PROTO_NUM=0;
	Head.D_NODE=p1->log.NEXT_d_node;
	Head.O_NODE=LocalAddr(rs->Conn.Socket,addr);
	Head.ERRNO1=rs->Conn.MTU;
	Head.ERRNO2=timeout;
	Head.PKG_REC_NUM=rs->cli.ctx_id;
	Head.data=buf;
	Head.PKG_LEN=strlen(Head.data);
	ret=SendPack(&rs->Conn,&Head);
	ret=RecvPack(&rs->Conn,&Head);
ShowLog(5,"%s:login return %s,ret=%d",__FUNCTION__,Head.data,ret);
	if(ret) { //网络失败
		rs->cli.Errno=errno;
		stptok(strerror(errno),rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		ShowLog(1,"%s:network error %d,%s",__FUNCTION__,rs->cli.Errno,rs->cli.ErrMsg);
		return -2;
	}
	if(Head.ERRNO1 || Head.ERRNO2) {  //login失败
		ShowLog(1,"%s:HOST=%s,login error ERRNO1=%d,ERRNO2=%d,%s",__FUNCTION__,
			rs->Conn.Host,Head.ERRNO1,Head.ERRNO2,Head.data);
		if(Head.ERRNO1==-197) { //ctx_id已经超时，失效了。
			rs->cli.ctx_id=0;
			goto again_login;
		}
errret:
		stptok(Head.data,rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		rs->cli.Errno=-1;
		return -3;
	}
	net_dispack(&logs,Head.data,log_tpl);
	strcpy(rs->cli.DBOWN,logs.DBOWN);
	strcpy(rs->cli.UID,logs.DBUSER);
	if(*logs.DBLABEL) {// rs->cli.ctx_id=atoi(logs.DBLABEL);
//ShowLog(5,"$s[%d]:DBLABEL=%s",__FUNCTION__,__LINE__,
		str_a64n(1,logs.DBLABEL,(u_int *)&rs->cli.ctx_id);
	}
//取服务名
	rs->cli.svc_tbl=&p1->svc_tbl;
	if(p1->svc_tbl.srvn == 0) {
		p1->svc_tbl.srvn=-1;
//DBLABEL?
	    ret=init_svc_no(&rs->Conn);
	    if(ret) { //取服务名失败
		ShowLog(1,"%s:HOST=%s,init_svc_no error ERRNO1=%d,ERRNO2=%d,%s",__FUNCTION__,
			&rs->Conn.Host,Head.ERRNO1,Head.ERRNO2,Head.data);
		goto errret;
	    }
	} else {
		p1->svc_tbl.usage++;
                rs->Conn.freevar=(void (*)(void *)) free_srv_list;
        }
	return 0;
}

//取连接  
T_Connect * get_RC_connect(int n,int flg)
{
int i,ret;
pool *pl;
resource *rs;

	if(!rcpool || n<0 || n>=RCPOOLNUM) return NULL;
	pl=&rcpool[n];
	if(!pl->lnk) {
		ShowLog(1,"%s:无效的连接池[%d]",__FUNCTION__,n);
		return NULL;
	}
	if(0!=pthread_mutex_lock(&pl->mut))  return (T_Connect *)-1;
	while(0>(i=get_lnk_no(pl))) {
	    if(flg) {	//flg !=0,don't wait
		pthread_mutex_unlock(&pl->mut);
		return NULL;
	    }
//	    if(log_level) ShowLog(log_level,"%s pool[%d] suspend",
//		__FUNCTION__,n);
	    pthread_cond_wait(&pl->cond,&pl->mut); //没有资源，等待 
//	    if(log_level) ShowLog(log_level,"%s tid=%lu pool[%d] weakup",
//		__FUNCTION__,pthread_self(),n);
	}
	pthread_mutex_unlock(&pl->mut);
	rs=&pl->lnk[i];
	rs->timestamp=now_usec();
	if(rs->Conn.Socket<0 || rs->cli.Errno<0) {
		ret=sc_connect(pl,rs);
		if(ret) {
			ShowLog(1,"%s:rcpool[%d].%d 连接%s/%s错:err=%d,%s",
				__FUNCTION__,n,i,pl->log.HOST,pl->log.PORT,
				rs->cli.Errno, rs->cli.ErrMsg);
			rs->cli.Errno=-1;
			pthread_mutex_lock(&pl->mut);
			add_lnk(pl,i);
			pthread_mutex_unlock(&pl->mut);
			return NULL;
		} 
	} 
	if(log_level) ShowLog(log_level,"%s:pool[%d].%d,USEC=%llu",__FUNCTION__,
			n,i,rs->timestamp);
	rs->cli.Errno=0;
	*rs->cli.ErrMsg=0;
	return &rs->Conn;
}
//归还连接  
void release_RC_connect(T_Connect **Connect,int n)
{
int i;
pool *pl;
resource *rs;
T_CLI_Var *clip;
	if(!Connect || !rcpool || n<0 || n>=RCPOOLNUM) {
		ShowLog(1,"%s:poolno=%d,错误的参数",__FUNCTION__,n);
		return;
	}
	if(!*Connect) {
		ShowLog(1,"%s:Conn is Empty!",__FUNCTION__);
		return;
	}
	(*Connect)->CryptFlg &= ~UNDO_ZIP;
	clip=(T_CLI_Var *)((*Connect)->Var);
	pl=&rcpool[n];
	i=lnk_no(pl,*Connect);
	if(i>=0) {
		rs=&pl->lnk[i];
		if(clip->Errno==-1) {  //连接失效
			ShowLog(1,"%s:rcpool[%d].%d to fail!",__FUNCTION__,n,i);
			disconnect(&rs->Conn);
		}
		clip->Errno=0;
		*clip->ErrMsg=0;
		
		pthread_mutex_lock(&pl->mut);
		add_lnk(pl,i);
		pthread_mutex_unlock(&pl->mut);
		pthread_cond_signal(&pl->cond); //如果有等待连接的线程就唤醒它 
  		rs->timestamp=now_usec();
		*Connect=NULL;
		if(log_level) ShowLog(log_level,"%s:pool[%d].%d,USEC=%llu",
					__FUNCTION__,n,i,rs->timestamp);
		return;
	}
	ShowLog(1,"%s:在pool[%d]中未发现该连接",__FUNCTION__,n);
	clip->Errno=0;
	*clip->ErrMsg=0;
	*Connect=NULL;
}
//连接池监控 
void rcpool_check()
{
int n,i,num;
pool *pl;
resource *rs;
INT64 now;
char buf[40];
//T_Connect *conn=NULL;

        if(!rcpool) return;
        now=now_usec();
        pl=rcpool;

        for(n=0;n<RCPOOLNUM;n++,pl++) {
                if(!pl->lnk) continue;
                rs=pl->lnk;
                num=pl->resource_num;
//              if(log_level) ShowLog(log_level,"%s:rcpool[%d],num=%d",__FUNCTION__,n,num);
                pthread_mutex_lock(&pl->mut);
                for(i=0;i<num;i++,rs++) if(rs->next >= 0) {
                        if(rs->Conn.Socket>-1 && (now-rs->timestamp)>299000000) {
//空闲时间太长了     
//                              if(0!=pthread_mutex_lock(&pl->mut)) continue;
ShowLog(1,"%s:rcpool[%d].%d,Socket=%d,to free",__FUNCTION__,n,i,rs->Conn.Socket);
//                              rs->Conn.Var=NULL; //引起内存泄漏？
                                disconnect(&rs->Conn);
                                rs->cli.Errno=-1;
                                if(log_level)
                                        ShowLog(log_level,"%s:Close RCpool[%d].%d,since %s",__FUNCTION__,
                                        n,i,rusecstrfmt(buf,rs->timestamp,YEAR_TO_USEC));
                        }
                } else {
                        if(rs->Conn.Socket>-1 && (now-rs->timestamp)>299000000) {
//占用时间太长了     
                                    if(log_level) ShowLog(log_level,"%s:rcpool[%d].lnk[%d] in used,since %s",
                                        __FUNCTION__,n,i,
                                        rusecstrfmt(buf,rs->timestamp,YEAR_TO_USEC));
                        }
                }
                pthread_mutex_unlock(&pl->mut);
        }
}

/**
 * 根据d_node取连接池号  
 * 失败返回-1
 */
int get_rcpool_no(int d_node)
{
int n;
	if(!rcpool) return -1;
	for(n=0;n<RCPOOLNUM;n++) {
		if(rcpool[n].d_node==d_node) return n;
	}
	return -1;
}

int get_rcpoolnum()
{
	return RCPOOLNUM;
}

char *get_rcLABEL(int poolno)
{
	if(poolno<0 || poolno>=RCPOOLNUM) return NULL;
	return rcpool[poolno].log.LABEL;
}

