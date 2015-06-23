#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <net.h>

extern int errno;
extern char *getenv(char *);
extern char LOGFILE[]; // showlog.c

int getlist(T_Connect *conn,T_NetHead *NetHead)
{
char *buff,*env,*p;
char *p1,*p2;
char tmp[1000],dir[512],tmp1[2000];
FILE *fd;
int ret,dirnum;
char buffer[4096];
	if(!NetHead->pkg_len) {
		ShowLog(1,"getlist what to list?");
     		NetHead->errno1=0;
errret:
		NetHead->data=0;
		NetHead->pkg_len=0;
     		NetHead->errno2=-1;
     		NetHead->pkg_rec_num=0;
     		NetHead->o_node=0;
     		NetHead->d_node=0;
		ret=SendPack(conn,NetHead);
		return 0;
	}
	buff=malloc(NetHead->pkg_len+100);
	strcpy(buff,NetHead->data);
ShowLog(5,"%s",buff);
	if(!strncmp(buff,"$/",2)) {
		env=getenv("SENDDIR");
		p=buff+2;
	} else if(!strncmp(buff,"@/",2)) {
		env=getenv("HOME");
		p=buff+2;
	} else if(*buff=='$') {
		p=stptok(buff+1,tmp,sizeof(tmp),"/");
		env=getenv(tmp);
	} else {
		p=buff;
	}
	if(p>buff) {
		if(!env||!*env) env=".";
		sprintf(tmp,"%s/",env);
		strsubst(buff,p-buff,tmp);
	}
	//if(!*LOGFILE) p2="/dev/null";
	//else p2=LOGFILE;
	p2="/dev/null";
	sprintf(tmp,"ls -R %s 2>>%s ",buff,p2);
	ShowLog(5,tmp);
	dirnum=0;
	*dir=0;
	fd=popen(tmp,"r");
	if(!fd) {
		ShowLog(1,"%s:err=%d",tmp,errno);
		free(buff);
		NetHead->errno1=errno;
		goto errret;
	}
	buffer[0]=0;
	while((ret=fscanf(fd,"%s",tmp))==1) {
		if(tmp[strlen(tmp)-1]==':') { // is dir
			if(!*dir) {
				tmp[strlen(tmp)-1]=0;
				strcpy(dir,tmp);
				continue;
			} else {
				ShowLog(5,tmp);
				break; // no -R
			}
		}
		if(*dir) {
			if(dir[strlen(dir)-1]=='/')
				sprintf(tmp1,"%s%s",dir,tmp);
			else sprintf(tmp1,"%s/%s",dir,tmp);
		} else {
			strcpy(tmp1,tmp);
		}
		if(isrfile(tmp1)<=0) continue; //not readble reg file
/* send tmp1 to client */
		ret=strlen(buffer);
		if((ret+strlen(tmp1)+2) < sizeof(buffer)) {
			sprintf(buffer+ret,"%s ",tmp1);
		} else {
			NetHead->data=buffer;
     			NetHead->pkg_len=strlen(NetHead->data)+1;
     			NetHead->errno2=0;
     			NetHead->pkg_rec_num=0;
     			NetHead->o_node=0;
     			NetHead->d_node=0;
			ret=SendPack(conn,NetHead);
			if(ret) break;
			strcpy(buffer,tmp1);
			strcat(buffer," ");
		}
	}
	pclose(fd);
	if(*buffer) {
		NetHead->data=buffer;
     		NetHead->pkg_len=strlen(NetHead->data)+1;
     		NetHead->errno2=0;
     		NetHead->pkg_rec_num=0;
     		NetHead->o_node=0;
     		NetHead->d_node=0;
		ret=SendPack(conn,NetHead);
		*buffer=0;
	}
	NetHead->data=0;
     	NetHead->pkg_len=0;
     	NetHead->errno2=100;
     	NetHead->pkg_rec_num=0;
     	NetHead->o_node=0;
     	NetHead->d_node=0;
	ret=SendPack(conn,NetHead);
	free(buff);
	return 0;
}
