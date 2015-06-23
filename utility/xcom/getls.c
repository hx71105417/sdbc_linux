#include "xcom.h"
#include <string.h>
void s_output(char *str,FILE *fd,char *localpath);

extern int substitute_env(char *line);
extern int NZ_Get_File(T_Connect *conn,char *local,char *remote);

int getls(T_Connect *conn,T_NetHead *NetHead,char *path,FILE *outfile)
{
int ret;
char *rpath,*localpath;
char tmp[100];
int Event_no=0;
	rpath=strtok(path," 	\r\n");
	if(!rpath||!*rpath) rpath=path,localpath=0;
	else {
		localpath=strtok(NULL," 	\r\n");
		if(localpath && (!*(localpath=skipblk(localpath))))
				localpath=0;
		else substitute_env(localpath); // !!!
	}
//printf("rpath=%s,localpath=%s\n",rpath,localpath);
	NetHead->data=rpath;
	NetHead->PKG_LEN=strlen(NetHead->data)+1;
	NetHead->ERRNO1=0;
	NetHead->ERRNO2=1;
	NetHead->PROTO_NUM=get_srv_no(conn->Var,"filels");
	ret=SendPack(conn,NetHead);
//printf("send ret=%d,err1=%d,err2=%d,data=%s\n",ret,NetHead->ERRNO1,NetHead->ERRNO2,NetHead->data);
	for(;;) {
		ret=RecvPack(conn,NetHead);
		if(NetHead->PROTO_NUM) Event_no=NetHead->PROTO_NUM;
//printf("ret=%d,err1=%d,err2=%d,data=%s\n",ret,NetHead->ERRNO1,NetHead->ERRNO2,NetHead->data);
		if(ret|| NetHead->ERRNO1) {
			EventCatch(conn,Event_no);
			ShowLog(1,"getls recv ret=%d,err1=%d,err2=%d,e_node=%s",
				ret,NetHead->ERRNO1,NetHead->ERRNO2,
				StrAddr(NetHead->O_NODE,tmp));
			return -1;
		}
		if(NetHead->ERRNO2!=PACK_CONTINUE) break;
		s_output(NetHead->data,outfile,localpath);
	}
	EventCatch(conn,Event_no);
	return 0;
}
void s_output(char *str,FILE *fd,char *localpath)
{
char buf[1024];
char *p;
	for(p=str;*(p=stptok(p,buf,sizeof(buf),"	 "));p++) {
		if(localpath && *localpath)
			fprintf(fd,"%s %s\n",buf,localpath);
		else fprintf(fd,"%s\n",buf);
	}
}

int m_getfile(T_Connect *conn,T_NetHead *NetHead,char *path)
{
int ret;
FILE *tmpfd;
	tmpfd=tmpfile();
	if(!tmpfd) {
		fprintf(stderr,"create tmpfile err %d\n",errno);
		return -1;
	}
	ret=getls(conn,NetHead,path,tmpfd);
	if(ret) {
		ShowLog(1,"m_getfile getls err %d\n",ret);
		return ret;
	}
	fseek(tmpfd,0L,0);
	ret=s_getfile(conn,tmpfd);
	fclose(tmpfd);
	return ret;
}
extern char *basename(char *);

int s_getfile(T_Connect *conn,FILE *inputfile)
{
char buf[2048];
int ret=0,i;
char sflnm[256],dflnm[256];
char *p,*p1;
	*sflnm=0;
	*dflnm=0;
	while(!ferror(inputfile)) {
		fgets(buf,sizeof(buf),inputfile);
		if(feof(inputfile) || ferror(inputfile)) break;
		if(*buf=='#') continue;
		ret=sscanf(buf,"%s%s",sflnm,dflnm);
		p=basename(sflnm);
		if(ret==1) {
			p1=getenv("RECVDIR");
			if(!p1||!*p1) p1="/tmp";
			if(p1[strlen(p1)-1]=='/') sprintf(dflnm,"%s%s",p1,p);
			else  sprintf(dflnm,"%s/%s",p1,p);
		}
		else if(dflnm[(i=strlen(dflnm))-1]=='/') {
			strcat(dflnm,p);
		} else if(isdir(dflnm)>0) {
				sprintf(dflnm+strlen(dflnm),"/%s",p);
		} else ;
		ret=NZ_Get_File(conn,dflnm,sflnm);
		if(ret) {
			ShowLog(1,"NZ_Get_File %s to %s ret=%d",
				sflnm,dflnm,ret);
		} else printf("%s\n",dflnm);
	}
	return ret;
}
