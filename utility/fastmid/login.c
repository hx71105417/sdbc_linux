#include <unistd.h>
#include "midsc.h"
#include <dw.h>
#include <scry.h>
#include <enigma.h>
#include <crc.h>
#include <crc32.h>

//#include <string.h>

extern srvfunc Function[];

struct login_s {
	char devid[17];
	char label[256];
	char CA[100];
	char uid[17];
	char pwd[14];
};
T_PkgType login_type[]={
	{CH_CHAR,17,"devid",0,-1},
	{CH_CHAR,256,"label"},
	{CH_CHAR,100,"CA"},
	{CH_CHAR,17,"uid"},
	{CH_CHAR,14,"pwd"},
	{-1,0}
};	

static int login_finish(T_Connect *conn,T_NetHead *NetHead);

int login(T_Connect *conn,T_NetHead *NetHead)
{
int ret,crc;
char tmp[200];
char *cp,*key;
char tmp1[1024],cliaddr[20];
DWS dw;
struct login_s logrec;
ENIGMA egm;
FILE *fd;
T_SRV_Var *up;
GDA *gp;
//u_int e[RSALEN],m[RSALEN];

	up=(T_SRV_Var *)conn->Var;
	gp=(GDA *)up->var;
	StrAddr(NetHead->O_NODE,cliaddr);
ShowLog(5,"%s:TCB:%d Client IP Addr=%s,Net_login %s",__FUNCTION__,up->TCB_no,cliaddr,NetHead->data);
	net_dispack(&logrec,NetHead->data,login_type);
	strcpy(gp->devid,logrec.devid);
	sprintf(gp->ShowID,"%s:%s:%d",logrec.devid,cliaddr,up->TCB_no);
	mthr_showid_add(up->tid,gp->ShowID);

	conn->MTU=NetHead->ERRNO1;

	cp=getenv("KEYFILE");
	if(!cp||!*cp) {
		strcpy(tmp1,"缺少环境变量 KEYFILE");
errret:
		ShowLog(1,"%s:Error %s",__FUNCTION__,tmp1);
		NetHead->ERRNO1=-1;
		NetHead->ERRNO2=-1;
		NetHead->PKG_REC_NUM=0;
		NetHead->data=tmp1;
		NetHead->PKG_LEN=strlen(NetHead->data);
    		SendPack(conn,NetHead);
		return 0; // fail
	}
/* read key */
	crc=0;
reopen:
	ret=initdw(cp,&dw);
	if(ret) {
		if((errno==24)&& (++crc<5)) {
			sleep(15);
			goto reopen;
		}
		sprintf(tmp1,"Init dw error %d",ret);
		goto errret;
	}
	crc=ssh_crc32((unsigned char *)logrec.devid,strlen(logrec.devid));
	key=getdw(crc,&dw);
	if(!key) {
		freedw(&dw);
                sprintf(tmp1,"无效的 DEVID");
                goto errret;
        }

//ShowLog(5,"getdw key=%s",key);
	enigma1_init(egm,key);
/* check CA */
	memset(gp->operid,0,sizeof(gp->operid));
	cp=getenv("CADIR");
	if(!cp||!*cp) cp=".";
    if(strcmp(gp->devid,"REGISTER")) {
	strncpy(gp->operid,logrec.uid,sizeof(gp->operid)-1);
	sprintf(tmp,"%s/%s.CA",cp,logrec.devid);
//ShowLog(5,"CAfile=%s,key=%s",tmp,key);
	fd=fopen(tmp,"r");
	if(!fd) {
		if(errno==2) {
		    crc=strlen(logrec.CA);
		    frenz_encode(egm,logrec.CA,crc);
		    byte_a64(tmp1,logrec.CA,crc);
//ShowLog(5,"CA=%s",tmp1);
		    fd=fopen(tmp,"w");
		    if(!fd) {
			sprintf(tmp1,"write %s err=%d",tmp,errno);
err1:
			freedw(&dw);
			goto errret;
		    }
		    fprintf(fd,"%s\n",tmp1);
		    fclose(fd);
		} else {
			sprintf(tmp1,"open CAfile %s err=%d",tmp,errno);
			goto err1;
		}
	} else {
		fgets(tmp1,sizeof(logrec.CA),fd);
		fclose(fd);
		TRIM(tmp1);
		ret=a64_byte(tmp,tmp1);
		frenz_decode(egm,tmp,ret);
		tmp[ret]=0;
		if(strcmp(tmp,logrec.CA)) {
			sprintf(tmp1,"CA 错误");
ShowLog(1,"%s:%s CA=%s log=%s len=%d",__FUNCTION__,tmp1,tmp,logrec.CA,ret);
			goto err1;
		}
	}
    } else {   //未注册客户端注册
	char *p;
	char *keyD;
/* REGISTER label|CA|devfile|CHK_Code| */

ShowLog(2,"REGISTER %s",logrec.uid);
	if(!*logrec.uid) {
		sprintf(tmp1,"REGSTER is empty!");
		goto err1;
	}
//uid=devfile
	crc=0xFFFF&gencrc((unsigned char *)logrec.uid,strlen(logrec.uid));
//pwd=CHK_Code
	sscanf(logrec.pwd,"%04X",&ret);
	ret &= 0xFFFF;
	if(ret != crc) {
		sprintf(tmp1,"REGISTER:devfile CHK Code error! ");//, crc,ret);
		goto err1;
	}
	p=stptok(logrec.uid,logrec.devid,sizeof(logrec.devid),".");//logrec.devid=准备注册的DEVID
	crc=ssh_crc32((unsigned char *)logrec.devid,strlen(logrec.devid));
	keyD=getdw(crc,&dw);
	if(!keyD) {
		sprintf(tmp1,"注册失败,%s:没有这个设备！",
				logrec.devid);
		goto err1;
	}
	enigma1_init(egm,keyD);
	sprintf(tmp,"%s/%s.CA",cp,logrec.devid);
ShowLog(5,"REGISTER:%s",tmp);
	if(0!=(fd=fopen(tmp,"r"))) {
		fgets(tmp1,81,fd);
		fclose(fd);
		TRIM(tmp1);
		ret=a64_byte(tmp,tmp1);
		frenz_decode(egm,tmp,ret);
		tmp[ret]=0;
		if(strcmp(tmp,logrec.CA)) {
			sprintf(tmp1,"注册失败,%s 已被注册,使用中。",
					logrec.devid);
			goto err1;
		}
	} else if(errno != 2) {
		sprintf(tmp1,"CA 错误");
		goto err1;
	}
/*把设备特征码写入文件*/
	fd=fopen(tmp,"w");
	if(fd) {
	int len=strlen(logrec.CA);
		frenz_encode(egm,logrec.CA,len);
		byte_a64(tmp1,logrec.CA,len);
		fprintf(fd,"%s\n",tmp1);
		fclose(fd);
	}
	else ShowLog(1,"net_login:REGISTER open %s for write,err=%d,%s",
		tmp,errno,strerror(errno));

	freedw(&dw);
	sprintf(tmp,"%s/%s",cp,logrec.uid);
	fd=fopen(tmp,"r");
	if(!fd) {
		sprintf(tmp1,"REGISTER 打不开文件 %s err=%d,%s",
					logrec.CA,errno,strerror(errno));
		goto errret;
	}
	fgets(logrec.uid,sizeof(logrec.uid),fd);
	TRIM(logrec.uid);
	ShowLog(2,"REGISTER open %s",tmp);
	fclose(fd);
	cp=tmp1;
	cp+=sprintf(cp,"%s|%s|", logrec.devid,logrec.uid);
	cp+=sprintf(cp,"%s|",rsecstrfmt(tmp,now_sec(),YEAR_TO_SEC));
	NetHead->data=tmp1;
	NetHead->PKG_LEN=strlen(NetHead->data);
	NetHead->ERRNO1=0;
	NetHead->ERRNO2=0;
	NetHead->PKG_REC_NUM=0;
    	SendPack(conn,NetHead);
	return -1;
    } //未注册客户端注册完成

	freedw(&dw);
	up->poolno=get_scpool_no(NetHead->D_NODE);
	if(up->poolno<0) {
		sprintf(tmp1,"非法的D_NODE %d",NetHead->D_NODE);
		goto errret;
	}
	ret=get_s_connect(up->TCB_no,up->poolno,&gp->server,login_finish);
	if(ret==0) return login_finish(conn,NetHead);
	else if(ret==1) return -5;
	sprintf(tmp1,"错误的参数");
	goto errret;
}

static int login_finish(T_Connect *conn,T_NetHead *NetHead)
{
char *cp;
T_SRV_Var *up=(T_SRV_Var *)conn->Var;
GDA *gp=(GDA *)up->var;
T_CLI_Var *clip;
char tmp[30],tmp1[256];

	unset_callback(up->TCB_no);
//	up->poolno=get_scpool_no(NetHead->D_NODE);
/* 怎么认证还得想办法
	cp=get_LABEL(up->poolno);
	if(!cp || strcmp(cp,logrec.label)) {
		sprintf(tmp1,"错误的DBLABEL %s",logrec.label);
		goto errret;
	}
*/
	if(!gp->server) {
		sprintf(tmp1,"%s:connect to server fault,TCB:%d",
			__FUNCTION__,up->TCB_no);
		ShowLog(1,"%s:Error:%s",__FUNCTION__,tmp1);
                NetHead->ERRNO1=-1;
                NetHead->ERRNO2=-1;
                NetHead->PKG_REC_NUM=0;
                NetHead->data=tmp1;
                NetHead->PKG_LEN=strlen(NetHead->data);
                SendPack(conn,NetHead);
                return 0; // fail
	}
	clip=(T_CLI_Var *)gp->server->Var;
	if(clip) {
//		stptok(clip->UID,up->SQL_Connect.UID,sizeof(up->SQL_Connect.UID),0);
//		stptok(clip->DBOWN,up->SQL_Connect.DBOWN,sizeof(up->SQL_Connect.DBOWN),0);
	}
ShowLog(5,"%s:TCB:%d,poolno=%d,Errno=%d",__FUNCTION__,up->TCB_no,up->poolno,clip->Errno);
	release_SC_connect(&gp->server,up->TCB_no,up->poolno);
	cp=tmp1;
	if(clip) cp+=sprintf(cp,"%s|%s|%s|%s|",
			gp->devid,gp->operid ,clip->UID,clip->DBOWN);

	else cp+=sprintf(cp,"%s|%s|||", gp->devid,gp->operid);
	cp+=sprintf(cp,"%s|%d|",rsecstrfmt(tmp,now_sec(),YEAR_TO_SEC),up->TCB_no);
	ShowLog(2,"%s:%s Login success",__FUNCTION__,tmp1);

	NetHead->data=tmp1;
	NetHead->PKG_LEN=strlen(NetHead->data);
	NetHead->ERRNO1=0;
	NetHead->ERRNO2=0;
	NetHead->PKG_REC_NUM=0;
    	SendPack(conn,NetHead);
	return 1;
}

