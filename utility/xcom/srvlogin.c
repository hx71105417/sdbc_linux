#include "xcsrv.h"
#include <bignum.h>
#include <dw.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
char *crypt();
int write_passwd(T_SQL_Connect *SQL_Connect,char *zyrid,char *passwd);
extern int errno;
int crypt_c(T_Connect *conn,char *str,int i);

int net_login(T_Connect *conn, T_NetHead *NetHead)
{
char *p,*cp,tmp[580];
char tmp1[1180];
char stmt[1024];
char sucret[80];
char rowid[20];
char cryptbuf[20],salt[3];
char *database;
int i,crc,ret;
int curno;
FILE *fd;
DWS dw;
struct dic_zyr_s  zyrrec;
struct dic_devadmin_s devadmin;
struct dic_userdef_s userdef;
struct dic_gwdevlog_s gwdevlog;
struct dic_tran_s tran;
T_Connect cryptconn;
T_User_Var *uservar;
GDA *gp;
/*   devname label|ca|zyrid|zyrpasswd| */
struct log_s  lg_pack;
struct in_addr cliaddr,dbaddr;
struct tm *tim;
time_t today;
char *netlogin;

	netlogin="net_login:";
	initconnect(&cryptconn);
	NetHead->errno1=0;
	uservar=(T_User_Var *)conn->Var;
	gp=(GDA *)uservar->var;
	if(!NetHead->pkg_len){
		strcpy(tmp1,"What LOG MSG?");
errret:
		NetHead->data=tmp1;
		NetHead->pkg_len=strlen(NetHead->data)+1;
		NetHead->errno2=1;
   	 	SendPack(conn,NetHead);
		return 0;
	}
	gp->pid=getpid();
	cp=getenv("KEYFILE");
	if(!cp) {
		strcpy(tmp1,"env KEYFILE not found!");
		ShowLog(1,tmp1);
		goto errret;
	}
	ret=initdw(cp,&dw);
	if(ret) {
		sprintf(tmp1,"initdw %s fail err=%d",cp,errno);
		ShowLog(1,tmp1);
		goto errret;
	}
/* get devics name */
	cp=gp->devname;
	p=NetHead->data;
	while((*p&255) > ' ') {
		*cp++=*p++;
	}
	*cp=0;
	Showid=gp->devname;
	crc=ssh_crc32(gp->devname,strlen(gp->devname));
	cp=getdw(crc,&dw);
	if(!cp) {
		sprintf(tmp1,"no such device %s in keyfile",
			gp->devname);
		ShowLog(1,tmp1);
		freedw(&dw);
		goto errret;
	}
	p++;
	strcpy(cryptconn.Cryptogram,cp);
/*****unpack ***********************************
 *   devname label|ca|zyrid|zyrpasswd| 
 *   get label
 ***********************************************/
	net_dispack((char *)&lg_pack,p,logview);
sprintf(tmp1,"UnPack:key=%s,LABEL=%s,user=%s,passwd=%s",
		cp,lg_pack.label,lg_pack.zyrid,lg_pack.zyrpasswd);
ShowLog(5,tmp1);
	i=a64_byte(tmp,lg_pack.label);
	crypt_password(cp+2,cp,tmp,i,&cryptconn);
	strcpy(lg_pack.label,tmp);
	freedw(&dw);
	sprintf(tmp,"DATABASE=%s",lg_pack.label);
	strcfg(tmp);
/* 检查访问权，或可信任主机及可信任用户 */
/**************************************************************
 *  open database 
 **************************************************************/
	database=getenv("DATABASE");
	strcpy(tmp,database);
	if(database){
		ret=SQL_AUTH(tmp,
			uservar->SQL_Connect.DSN,
			uservar->SQL_Connect.UID,
			uservar->SQL_Connect.PWD
			);
		if(ret) {
			sprintf(tmp1,"%s SQL_AUTH fail:%d\n",
					database,ret);
			ShowLog(1,tmp1);
			goto errret;
		} else {
			sprintf(gp->showid,"%s:%s:%u",
				gp->devname,
				uservar->SQL_Connect.UID,
				gp->pid);
			Showid=gp->showid;
			sprintf(tmp1,"PID=%u,DB:%s,SID:%s,UID:%s,PSSWD:%s",
			gp->pid,
			database,
			uservar->SQL_Connect.DSN,
			uservar->SQL_Connect.UID,
			uservar->SQL_Connect.PWD);
			ShowLog(5,tmp1);
		}
					
	} else {
		sprintf(tmp1,"env DATABASE not found!");
		ShowLog(1,tmp1);
		goto errret;
	}
/*
	sprintf(tmp1,"ORACLE_SID=%s","0ra817");
	strcfg(tmp1);
	sprintf(tmp1,"USER=%s",uservar->SQL_Connect.UID);
	strcfg(tmp1);
*/
	ret=___SQL_OpenDatabase__( &uservar->SQL_Connect);
	if(ret) {
		sprintf(tmp1,"OpenDatabase %s fail:%d\n",tmp,ret);
		ShowLog(1,tmp1);
		goto errret;
	}
	uservar->dbown=getenv("DBOWN");
	if(!uservar->dbown || !*uservar->dbown) {
		uservar->dbown=uservar->SQL_Connect.UID; 
		sprintf(tmp1,"DBOWN=%s",uservar->dbown);
		strcfg(tmp1);
	}
	ret=___SQL_Transaction__(&(uservar->SQL_Connect),TRANBEGIN);
	ret=inet_aton(peeraddr(0,buf),&cliaddr);
	sprintf(stmt,"select %s from %s.dic_dev_admin where devid='%s'\n",
			mkfield(tmp,dic_devadmin_type),uservar->dbown,
				gp->devname); 
	strcpy(tmp,netlogin);
	ret=SelectOne(&uservar->SQL_Connect,stmt,tmp);
	if(ret) {
		strcpy(tmp1,tmp);
		ShowLog(1,tmp1);
dberr:
		ret=___SQL_Transaction__(&(uservar->SQL_Connect),TRANROLLBACK);
		___SQL_CloseDatabase__(&uservar->SQL_Connect);
		goto errret;
	}
	net_dispack(&devadmin,tmp,dic_devadmin_type);
	if(!*lg_pack.ca) { 
/*   check client addr */
		ret=inet_aton(devadmin.devaddr,&dbaddr);
		if(dbaddr.s_addr != cliaddr.s_addr) {
			strcpy(tmp1,"Client Addr Error");
			ShowLog(1,tmp1);
			goto dberr;
		}

	} else {  // Check CA 
		sprintf(tmp1,"%s is not CA!",lg_pack.ca);
		ShowLog(1,tmp1);
		goto dberr;
		if(!*devadmin.cafile) {
			sprintf(tmp1,"%s is not CA!",lg_pack.ca);
			ShowLog(1,tmp1);
			goto dberr;
		}
		cp=getenv("CADIR");
		sprintf(tmp,"%s/%s",cp?cp:".",devadmin.cafile);
		ret=access(tmp,F_OK);
		if(ret==-1) {
			fd=fopen(tmp,"w");
			if(!fd) {
				sprintf(tmp1,"Can not open cafile %s for write,err=%d!",
					tmp,errno);
				ShowLog(1,tmp1);
				goto dberr;
			}
			ret=strlen(lg_pack.ca);
			crypt_c(&cryptconn,lg_pack.ca,ret);
			byte_a64(tmp,lg_pack.ca,ret);
			fprintf(fd,"%s\n",tmp);
			fclose(fd);
		} else {
			fd=fopen(tmp,"r");
			if(!fd) {
				sprintf(tmp1,"Can not open cafile %s for read,err=%d!",
					tmp,errno);
				ShowLog(1,tmp1);
				goto dberr;
			}
			fsacnf(fd,"%s",stmt);
			ret=a64_byte(tmp,stmt);
			crypt_c(&cryptconn,tmp,ret);
			if(strcmp(tmp,lg_pack.ca)) {
				sprintf(tmp1,"CAerr:%s,%s",tmp,lg_pack.ca);
				ShowLog(1,tmp1);
				goto dberr;
			}
		}
	}
/* get zyrid */
	sprintf(stmt,"select %s from %s.dic_zyr where zyrid='%s'\n",
		mkfield(tmp,dic_zyr_type),uservar->dbown,lg_pack.zyrid); 
ShowLog(5,stmt);
	strcpy(tmp,netlogin);
	ret=SelectOne(&uservar->SQL_Connect,stmt,tmp);
	if(ret) {
		strcpy(tmp1,tmp);
		ShowLog(1,tmp1);
		goto dberr;
	}
	net_dispack(&zyrrec,tmp,dic_zyr_type);
/*  user allow? */
	if(!zyrrec.flag) {
		sprintf(tmp1,"user %s is not allowed",zyrrec.zyrid);
		ShowLog(1,tmp1);
		goto dberr;
	}
/* check password */
	if(*zyrrec.passwd) {
sprintf(tmp1,"user %s passwd:%s",zyrrec.zyrid,zyrrec.passwd);
ShowLog(5,tmp1);
		cp=crypt(lg_pack.zyrpasswd,zyrrec.passwd);
		if(strcmp(cp,zyrrec.passwd)) {
			sprintf(tmp1,"user passwd error!");
			ShowLog(1,tmp1);
			goto dberr;
		}
	} else {
/* write passwd into database */
		ret=write_passwd(&uservar->SQL_Connect,zyrrec.zyrid,
					lg_pack.zyrpasswd);
		if(ret) {
			sprintf(tmp1,"write_passwd=%d, err %s",ret,
				uservar->SQL_Connect.SqlError.szError);
			goto dberr;
		}
	}
/* get userdef and gwdevlog */
	sprintf(stmt,"select a.rowid,%s,%s from %s.dic_gw_dev_log a,%s.dic_user_def b where b.oraname='%s' and b.devid='%s' and b.zyrid='%s' and a.devid=b.devid and a.gw=b.gw",
			MkTabnameField(tmp,dic_gwdevlog_type,"a"),
			MkTabnameField(tmp1+900,dic_userdef_type,"b"),
			uservar->dbown, uservar->dbown,
			uservar->SQL_Connect.UID, gp->devname, lg_pack.zyrid); 
ShowLog(5,stmt);
			
	strcpy(tmp,netlogin);
	ret=SelectOne(&uservar->SQL_Connect,stmt,tmp);
	if(ret) {
		strcpy(tmp1,tmp);
		ShowLog(1,tmp1);
		goto dberr;
	}
/*get dic_gw_dev_log:rowid */
	CharType[0].len=19;
	p=tmp;
	p+=net_dispack(rowid,p,CharType);
/*get dic_gw_dev_log */
	p+=net_dispack(&gwdevlog,p,dic_gwdevlog_type);
	sprintf(tmp1,"gwdevlog.devid=%s,zyrid=%s",
		gwdevlog.devid,gwdevlog.zyrid);
	ShowLog(5,tmp1); 
/* get userdef */
	time(&today);
	tim=localtime(&today);
	strcpy(sucret,p);
	net_dispack(&userdef,p,dic_userdef_type);
	gp->gw=userdef.gw;
	strcpy(gp->zyrid,userdef.zyrid);
	sprintf(tmp1,"%s,%s,%s:gw=%d",userdef.oraname,userdef.devid,
			userdef.zyrid,userdef.gw);
	ShowLog(5,tmp1);
	if(!*gwdevlog.zyrid) {
/* auto open shift */
		sprintf(gwdevlog.kbsj,"%04d%02d%02d %02d:%02d",
			tim->tm_year+1900,tim->tm_mon+1,tim->tm_mday,
			tim->tm_hour,tim->tm_min);
		
		strcpy(gwdevlog.bb,"BB");  // BB='B' or 'Y'

		strcpy(gwdevlog.zyrid,userdef.zyrid);
		net_pack(tmp1+800,&gwdevlog,dic_gwdevlog_type);
		sprintf(stmt,"UPDATE %s.dic_gw_dev_log %s where rowid='%s'",
			uservar->dbown,
			mkupdate(tmp,tmp1+800,dic_gwdevlog_type),rowid);
		ShowLog(5,stmt);
		ret=___SQL_Exec(&(uservar->SQL_Connect),stmt);
		if(ret<=0) {
			sprintf(tmp1,"ret=%d,stmt=%s, err %d %s",ret,stmt,
				uservar->SQL_Connect.SqlError.nErrorMsg,
				uservar->SQL_Connect.SqlError.szError);
			ShowLog(1,tmp1);
		goto dberr;
		}
	} else {
		if(strcmp(gwdevlog.zyrid,gp->zyrid)) {
			sprintf(tmp1,"UID != gwdevlog.zyrid,%s != %s",
			gwdevlog.zyrid,gp->zyrid);
			goto dberr;
		}
	}
	p=sucret+strlen(sucret);
	*tran.str=0;
	sprintf(stmt,"select %s from %s.dic_tran where item=0 and key=1",
		mkfield(tmp,dic_tran_type),uservar->dbown);
	strcpy(tmp,"tran 1");
	ret=SelectOne(&uservar->SQL_Connect,stmt,tmp);
	if(!ret) {
		net_dispack((char *)&tran,tmp,dic_tran_type);
		TRIM((unsigned char *)tran.str);
		tran.str[3]=0;
	}
	strcpy(gp->bzdm,tran.str);
	p+=sprintf(p,"%s|",tran.str);
	*tran.str=0;
	sprintf(stmt,"select %s from %s.dic_tran where item=0 and key=2",
		mkfield(tmp,dic_tran_type),uservar->dbown);
	strcpy(tmp,"tran 2");
	ret=SelectOne(&uservar->SQL_Connect,stmt,tmp);
	if(!ret) {
		net_dispack((char *)&tran,tmp,dic_tran_type);
		TRIM((unsigned char *)tran.str);
		tran.str[10]=0;
	}
	strcpy(gp->bzzm,tran.str);
	p+=sprintf(p,"%s|",tran.str);


/*********************** Success Login ****************************/
success:
	ret=___SQL_Transaction__(&(uservar->SQL_Connect),TRANCOMMIT);
	p+=sprintf(p,"%04d-%02d-%02d %02d:%02d:%02d|",
		tim->tm_year+1900,tim->tm_mon+1,tim->tm_mday,
		tim->tm_hour,tim->tm_min,tim->tm_sec);
	p+=sprintf(p,"%s|",uservar->dbown);
	NetHead->data=sucret;
	NetHead->pkg_len=strlen(NetHead->data)+1;
	NetHead->errno2=0;
	NetHead->errno1=0;
	NetHead->pkg_len=strlen(NetHead->data)+1;
    	SendPack(conn,NetHead);
	return 1; /* 1:success,0:fault */
}


int write_passwd(T_SQL_Connect *SQL_Connect,char *zyrid,char *passwd)
{
char stmt[256],tmp1[556];
int ret;
char salt[3],passbuf[20];
	getsalt(salt);
	des_fcrypt(passwd,salt,passbuf);
/* write passwd into database */
	sprintf(stmt,"update %s.dic_zyr set passwd='%s' where zyrid='%s'",
			getenv("DBOWN"),passbuf,zyrid);
	ret=___SQL_Transaction__(SQL_Connect,TRANBEGIN);
	SQL_Connect->SqlError.nErrorMsg=0;

	ret=___SQL_Exec(SQL_Connect,stmt);
	//if(ret<=0||SQL_Connect->SqlError.nErrorMsg) {
	if(ret!=1) {
		sprintf(tmp1,"write_passwd=%d,stmt=%s; err %d %s",ret,stmt,
				SQL_Connect->SqlError.nErrorMsg,
				SQL_Connect->SqlError.szError);
		ShowLog(1,tmp1);
		ret=___SQL_Transaction__(SQL_Connect,TRANROLLBACK);
	}
	ret=___SQL_Transaction__(SQL_Connect,TRANCOMMIT);
//	return SQL_Connect->SqlError.nErrorMsg;
	return  0;
}

int change_passwd(T_Connect *conn, T_NetHead *NetHead)
{
int ret;
char tmp1[256];
T_User_Var *up;
GDA *gp;
	if(!NetHead->pkg_len) {
		sprintf(tmp1,"What Passwd?");
errret:
		NetHead->data=tmp1;
		NetHead->pkg_len=strlen(NetHead->data)+1;
		NetHead->errno1=0;
		NetHead->errno2=2;
		ret=SendPack(conn,NetHead);
		if(ret) ShowLog(1,tmp1);
		return 1;
	}
	up=(T_User_Var *)conn->Var;
	gp=(GDA *)up->var;
	ret=write_passwd(&up->SQL_Connect,gp->zyrid,NetHead->data);
	if(ret) {
		sprintf(tmp1,"write_passwd=%d, err %s",ret,
			up->SQL_Connect.SqlError.szError);
		ShowLog(1,tmp1);
		goto errret;
	}
	sprintf(tmp1,"change passwd %s success!",NetHead->data);
	ShowLog(5,tmp1);
	NetHead->data=tmp1;
	NetHead->pkg_len=strlen(NetHead->data)+1;
	NetHead->errno1=0;
	NetHead->errno2=0;
	ret=SendPack(conn,NetHead);
	if(ret) ShowLog(1,tmp1);
	return 0;
}
int cancel_passwd(T_Connect *conn, T_NetHead *NetHead)
{
int ret;
char tmp1[256],stmt[1024];
T_User_Var *up;
GDA *gp;
	if(!NetHead->pkg_len) {
		sprintf(tmp1,"What user?");
errret:
		NetHead->data=tmp1;
		NetHead->pkg_len=strlen(NetHead->data)+1;
		NetHead->errno1=0;
		NetHead->errno2=2;
		ret=SendPack(conn,NetHead);
		if(ret) ShowLog(1,tmp1);
		return 1;
	}
	up=(T_User_Var *)conn->Var;
	gp=(GDA *)up->var;
	if(strcmp(up->SQL_Connect.UID,"zd")) {
		sprintf(tmp1,"cancel passwd for %s by %s/%s:permssion denied",
			NetHead->data,up->SQL_Connect.UID,gp->zyrid);
	
		ShowLog(1,tmp1);
		NetHead->errno1=0;
		NetHead->errno2=6;
		NetHead->data=tmp1;
		NetHead->pkg_len=strlen(NetHead->data)+1;
		SendPack(conn,NetHead);
		return 0;
	}
	sprintf(stmt,"update %s.dic_zyr set passwd='' where zyrid='%s'",
			up->dbown,NetHead->data);
	up->SQL_Connect.SqlError.nErrorMsg=0;

	ret=___SQL_Exec(&up->SQL_Connect,stmt);
	if(ret!=1) {
		sprintf(tmp1,"cancle_passwd=%d,stmt=%s; err %d %s",ret,stmt,
				up->SQL_Connect.SqlError.nErrorMsg,
				up->SQL_Connect.SqlError.szError);
		ShowLog(1,tmp1);
		return -1;
	}
	sprintf(tmp1,"cancel passwd for %s by %s success!",
			NetHead->data,gp->zyrid);
	ShowLog(1,tmp1);
	NetHead->data=tmp1;
	NetHead->pkg_len=strlen(NetHead->data)+1;
	NetHead->errno1=0;
	NetHead->errno2=0;
	ret=SendPack(conn,NetHead);
	strcat(tmp1,"return send err");
	if(ret) ShowLog(1,tmp1);
	return 0;
}
int crypt_c(T_Connect *conn,char *str,int i)
{
char *cp;
	cp=conn->Cryptogram;
	crypt_password(cp+2,cp,str,i,conn);
	return i;
}
