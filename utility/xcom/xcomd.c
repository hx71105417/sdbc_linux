#include "xcomd.h"
#include <bignum.h>
#include <enigma.h>
#include <unistd.h>
#include <libgen.h>
#include <crc32.h>
#include <sys/resource.h>

int net_login(T_Connect *,T_NetHead *);
extern int substitute_env(char *line);

void showhex(int level,char *title,char *str,int i);

/*********************************************************
 * Can add other functions into Function[] by user       *
 *********************************************************/

//int (*Function[])(T_Connect *,T_NetHead *)={
srvfunc Function[]={
	{net_login,"NetLogin"},		/*0*/
	{Echo,"Echo"},			/*1*/
	{Rexec,"Rexec"},
	{GetFile,"GetFile"},
	{PutFile,"PutFile"},
	{Pwd,"Pwd"},
	{ChDir,"ChDir"},
	{filels,"filels"},
	{PutEnv,"PutEnv"},	
	{Z_PutFile,"Z_PutFile"},
	{Z_GetFile,"Z_GetFile"},
	{0,0}
};
void quit(int);
void netinit(T_Connect *connect ,T_NetHead *NetHead);
extern FILE *logfile;
long today;

static char myshowid[100];
int main(int ac,char *av[])
{
char *cp;
int ret;
struct rlimit sLimit;

//设置可以core dumpped
        sLimit.rlim_cur = -1;
        sLimit.rlim_max = -1;
        ret=setrlimit(RLIMIT_CORE,(const struct rlimit *)&sLimit);

	if(ac>1){
		ret=envcfg(av[1]);
	}

	tzset();
	cp=getenv("WDIR"); //取工作目录 
	if(!cp || !*cp) cp=getenv("HOME");
	if(cp && *cp) chdir(cp);
	sprintf(myshowid,"%s:%d",sc_basename(av[0]),getpid());
	Showid=myshowid;
ShowLog(2,"start!");
	ret=TPOOL_srv(netinit,quit,NULL,sizeof(GDA));

	quit(ret);
	return 0;
}
void quit(int n)
{
	ShowLog(0,"quit %d",n);
	ShowLog(-1,0);
	tpool_free();
	exit(n);
}

void FreeVar(void *P)
{
}

void netinit(T_Connect *connect ,T_NetHead *NetHead)
{
char addr[16],*cp;
int ret;
//T_SRV_Var *srvp=(T_SRV_Var *)connect->Var;
//GDA *ctx=(GDA *)srvp->var;

	connect->freevar=FreeVar;
	connect->Event_proc=event_put;//事件处理器 
	cp=getenv("SENDSIZE");
	if(cp && *cp) {
		ret=atoi(cp);
		connect->MTU=ret;
	}
	peeraddr(connect->Socket,addr);
	ShowLog(0,"连接 %s`",addr);
}

#include <dw.h>
struct log_s {
	char devid[17];
	char label[51];
	char CA[180];
};
T_PkgType login_type[]={
	{CH_CHAR,17,"devid",0,-1},
	{CH_CHAR,51,"label"},
	{CH_CHAR,180,"CA"},
	{-1,0,0}
};
int net_login(T_Connect *conn,T_NetHead *NetHead)

{
T_SRV_Var *srvp=(T_SRV_Var *)conn->Var;
GDA *gp=(GDA *)srvp->var;
time_t tim;
int ret;
char err[100],tmp[256],tmp1[1024];
char *p,*cp,*key;
DWS dw;
struct log_s logrec;
int crc;
FILE *fd;
ENIGMA2 egm;

	time(&tim);
	ret=1;
	if(!NetHead->PKG_LEN) {
		ShowLog(1,"log is empty!");
		return -1;
	}
	if((int)NetHead->ERRNO1>0) conn->MTU=NetHead->ERRNO1;
	if((int)NetHead->ERRNO2>0) conn->timeout=NetHead->ERRNO2;
	
	ShowLog(3,"login:%s,len=%d,%s",StrAddr(NetHead->O_NODE,err),
			NetHead->PKG_LEN,NetHead->data);
	p=NetHead->data;	
	p+=net_dispack(&logrec,p,login_type);
	peeraddr(conn->Socket,err);
	sprintf(gp->Showid,"%s:%s:%d",logrec.devid,err,srvp->TCB_no);
	mthr_showid_add(srvp->tid,gp->Showid);

	cp=getenv("KEYFILE");
        if(!cp||!*cp) {
                strcpy(tmp1,"缺少环境变量 KEYFILE");
errret:
                ShowLog(1,"Login Error %s",tmp1);
                NetHead->ERRNO1=-1;
                NetHead->ERRNO2=-1;
                NetHead->O_NODE=LocalAddr(conn->Socket,err);
                NetHead->PKG_REC_NUM=0;
                NetHead->data=tmp1;
                NetHead->PKG_LEN=strlen(NetHead->data);
                SendPack(conn,NetHead);
                return 0; // fail
        }
/* read key */
        ret=initdw(cp,&dw);
        if(ret) {
                sprintf(tmp1,"Init dw error %d",ret);
                goto errret;
        }
        crc=ssh_crc32((unsigned char *)logrec.devid,strlen(logrec.devid));
        key=getdw(crc,&dw);
        if(!key) {
                freedw(&dw);
                sprintf(tmp1,"无效的 DEVID:%s",logrec.devid);
                goto errret;
        }
	enigma2_init(&egm,key,0);

	ret=a64_byte(tmp1,logrec.label);
	enigma2_decrypt(&egm,tmp1,ret);
        tmp1[ret]=0;
	strcpy(logrec.label,tmp1);

	cp=getenv("CADIR");
	if(!cp||!*cp) cp=".";
	sprintf(tmp,"%s/%s.CA",cp,logrec.devid);
ShowLog(5,"CAfile=%s",tmp);
	fd=fopen(tmp,"r");
	if(!fd) {
	    if(errno==2) { //CA文件不存在
                fd=fopen(tmp,"w");
                if(!fd) {
                        sprintf(tmp1,"write %s err=%d",tmp,errno);
                	fclose(fd);
err1:
                        freedw(&dw);
                        goto errret;
                }
//写入CA文件
ShowLog(5,"CA=%s",logrec.CA);
		enigma2_encrypt(&egm,logrec.CA,crc=strlen(logrec.CA));
		byte_a64(tmp,logrec.CA,crc);
                fprintf(fd,"%s\n",tmp);
                fclose(fd);
             } else {
                sprintf(tmp1,"open CAfile %s err=%d",tmp,errno);
		goto err1;
             }
	} else {
		fgets(tmp,sizeof(tmp),fd);
        	fclose(fd);
        	TRIM(tmp);
		ret=a64_byte(err,tmp);
		enigma2_decrypt(&egm,err,ret);
        	err[ret]=0;
        	if(strcmp(err,logrec.CA)) {
                	sprintf(tmp1,"CA 错误");
ShowLog(5,"%s CA=%s log=%s len=%d",tmp1,tmp,logrec.CA,ret);
                        goto err1;
        	}
	}

        freedw(&dw);
//tmp1是label,实际上是工作目录
	substitute_env(tmp1);

ShowLog(3,"XCOMD login succeed:%s",tmp1);
	NetHead->data=tmp1;
	NetHead->PKG_LEN=strlen(NetHead->data);
	NetHead->ERRNO1=0;
	NetHead->ERRNO2=0;
	NetHead->PKG_REC_NUM=0;
	NetHead->O_NODE=LocalAddr(conn->Socket,err);
ShowLog(5,"O_NODE=%s %s",err,NetHead->data);
//showhex(5,err,(char *)&NetHead->O_NODE,4);
    	SendPack(conn,NetHead);
	return ret;
}
void showhex(int level,char *title,char *str,int i)
{
int j;
char errbuf[400];
char *p;
	strcpy(errbuf,title);
	strcat(errbuf," ");
	p=errbuf+strlen(errbuf);
	while(i>64) {
		i-=64;
		for(j=0;j<64;j++) p+=sprintf(p,"%02X ",*str++ & 255);
		ShowLog(level,"%s",errbuf);
		p=errbuf;
	}
	for(j=0;j<i;j++) p+=sprintf(p,"%02X ",*str++ & 255);
	ShowLog(level,"%s",errbuf);
}

void set_showid(void *ctx)
{
GDA *gp=(GDA *)ctx;
pthread_t tid=pthread_self();
        if(!ctx) return;
        mthr_showid_add(tid,gp->Showid);
}

