#include <ctype.h>
#include "midware.h"
#include <bignum.h>
#include <dw.h>
#include <sys/resource.h>

/***************************************************
 * Obj.txt:
 * OBJNO server|port|nextobjno|nextMTU|nextFamily|
 ***************************************************/
T_PkgType Object_type[]={
	{CH_CHAR,81,"server",0,-1},
	{CH_CHAR,21,"port"},
	{CH_INT,sizeof(int),"nextobj"},
	{CH_INT,sizeof(int),"nextMTU"},
	{CH_CHAR,172,"nextfml"},
	{-1,0,0}
};

struct Object_s {
	char server[81]; 	// server host name 
	char port[21];		// server service port
	int nextobj;        // used as next D_NODE
	int nextMTU;        // used as next MTU
	char nextfml[172];
	int object_no;
};

typedef struct Object_s T_Object;
void quit(int);
void netinit();
int uabin(char *p);
char *phex(char *p,int len);
extern char *getenv();

int net_login(T_Connect *conn, T_NetHead *NetHead);

extern u_int family[];

//int (*Function[])()={
srvfunc Function[]={
	{net_login,"net_login"},			/*0*/
	{Echo,"Echo"},
	{0,0}
};
static char myshowid[200];

void FreeVar(void *p)
{
T_SRV_Var *srvp=(T_SRV_Var *)p;
GDA *gp;
	if(srvp) {
		gp=(GDA *)srvp->var;;
		ShowLog(2,"disconnect %s/%s",gp->server.Host,gp->server.Service);
		disconnect(&gp->server);
	}
}

void netinit(conn,head)
T_Connect *conn;
T_NetHead *head;
{
T_SRV_Var *srvp=(T_SRV_Var *)conn->Var;
GDA *gp=(GDA *)srvp->var;
char addr[16];

	peeraddr(conn->Socket,addr);
	ShowLog(0,"连接 %s",addr);
	initconnect(&gp->server);
	conn->only_do=Transfer;
	conn->freevar=FreeVar;
}

int main(int ac,char *av[])
{
int i;
struct rlimit sLimit;

//设置可以core dumpped
        sLimit.rlim_cur = -1;
        sLimit.rlim_max = -1;
        i=setrlimit(RLIMIT_CORE,(const struct rlimit *)&sLimit);

	if(ac>1){
		envcfg(av[1]);
	}
	strcpy(myshowid,sc_basename(av[0]));
	Showid=myshowid;
	TPOOL_srv(netinit,quit,0,sizeof(GDA));
	quit(0);
	return 0;
}

void quit(int n)
{
	ShowLog(0,"quit %d!\n",n);
/***********************************
 * close logfile
 ***********************************/
	ShowLog(-1,0);
	if(n<0) n=-n;
	tpool_free();
	exit(n&255);
}

/* test family
static void prtfml(u_int *n,int rsalen)
{
int i;
char buf[512],*p;
#define keyfd 5
	strcpy(buf,"u_int family[]={");
	p=buf;
        for(i=0;i<rsalen;i++) {
                if(!(i%5)) {
			ShowLog(keyfd,"%s",buf);
			*buf=0;
			p=buf;
		}
                if(i<rsalen-1) p+=sprintf(p,"%u,",n[i]);
                else p+=sprintf(p,"%d};",n[i]);
        }
	ShowLog(5,"%s",buf);
}
*/
int net_login(T_Connect *conn, T_NetHead *NetHead)
{
T_SRV_Var *srvp=(T_SRV_Var *)conn->Var;
GDA *gp=(GDA *)srvp->var;
char *p,*cp,tmp[180];
int i,object_no;
extern int errno;
int loginflg=0;
T_Object obj;
DWS objdw;


        object_no=NetHead->D_NODE;
	if(NetHead->ERRNO2>0) conn->MTU=NetHead->ERRNO2;
/*********************************************************
 * read permission file
 *********************************************************/
	StrAddr(NetHead->O_NODE,tmp);
	p=getenv("OBJFILE");
	if(!p || !*p) p="obj.txt";
	i=initdw(p,&objdw);
	if(i) {
		sprintf(tmp,"Can't open %s i=%d,errno=%d,%s",
				p,i,errno,strerror(errno));
		ShowLog(1,"initdw:%s",tmp);
		i=-1;
		goto err;
	}
	cp=getdw(object_no,&objdw);
	if(!cp) {
		sprintf(tmp,"Can't find obj_no %d in File %s",object_no,p);
		ShowLog(1,"net_login getdw:%s",tmp);
		i=-1;
		freedw(&objdw);
		goto err;
	}
	net_dispack(&obj,cp,Object_type);
	if(obj.nextMTU<0) obj.nextMTU=0;
	obj.object_no=object_no;
	freedw(&objdw);
	p=stptok(NetHead->data,gp->Showid,sizeof(gp->Showid),"|");
	sprintf(gp->Showid+strlen(gp->Showid),".%d:%d-%s/%s.%d",srvp->TCB_no,
		obj.object_no, obj.server,obj.port,obj.nextobj);
	mthr_showid_add(srvp->tid,gp->Showid);
ShowLog(5,"mid login obj_no=%d,data=%s",object_no,NetHead->data);
	
/*****************************************************************
 * connect to server
 * 只能使用 from_srv->Service 和 from_srv->Host
 * 其他内容都会在 Net_Connect() 中被清除。
 *****************************************************************/
	strcpy(gp->server.Service,obj.port);
	strcpy(gp->server.Host,obj.server);
	gp->server.timeout=-1;
ShowLog(5,"mid login connect srv %s/%s",gp->server.Host,gp->server.Service);
	if(!*obj.nextfml) i=Net_Connect(&gp->server,0,0);
	else {
		str_a64n(32,obj.nextfml,gp->newfamily);
//prtfml(newfamily,32);
		i=Net_Connect(&gp->server,0,gp->newfamily);
	}
ShowLog(5,"mid login connect srv status=%d",i);
        if(i<0){
		sprintf(tmp, "mid Login connect to srv %s:%s/err:%d errno:%d",
			obj.server,obj.port,i,errno);
		ShowLog(1,"%s",tmp);
err:
		NetHead->ERRNO1=-1;
		NetHead->ERRNO2=i;
		NetHead->O_NODE=LocalAddr(conn->Socket,0);
		NetHead->data=tmp;
		NetHead->PKG_LEN=sizeof(tmp);
   	 	i=SendPack(conn,NetHead);
		return 0;
	}

	p=getenv("TIMEOUT");
	if(p&&*p) gp->server.timeout=atoi(p)*60;
	gp->server.Var=NULL;
	if(obj.nextMTU>0) {
		gp->server.MTU=obj.nextMTU;
		NetHead->ERRNO2=gp->server.MTU;
	}
	NetHead->D_NODE=obj.nextobj;
	i=SendPack(&gp->server,NetHead);
	if(i){
		sprintf(tmp,"send failerr=%d,errno=%d,%s",
			i,errno,strerror(errno));
		ShowLog(1,"net_login %s",tmp);
		goto err;
	} else i=RecvPack(&gp->server,NetHead);
	if(i){
		sprintf(tmp,"recv from srv fail %d errno=%d,%s",
			i,errno,strerror(errno));
		ShowLog(1,"net_login %s",tmp);
		goto err;
	} 
	ShowLog(5,"Login Recv from srv:%d,errno2=%d,data=%s",
			i,NetHead->ERRNO2,NetHead->data);
	loginflg=(NetHead->ERRNO2==0);
	i=SendPack(conn,NetHead);
	return loginflg; /* 1:success,0:fault */
}

void set_showid(void *ctx)
{
GDA *gp=(GDA *)ctx;
pthread_t tid=pthread_self();
        if(!ctx) return;
        mthr_showid_add(tid,gp->Showid);
}

