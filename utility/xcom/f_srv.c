#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "xcomd.h"
#include <md5.h>
extern char *getenv();
extern FILE *logfile;

#define REPEAT 3


extern int substitute_env(char *line);

#include <sys/stat.h>
/*************************************************/
/* µÃµ½ÎÄ¼þ³¤¶È                                  */
/*     Óï·¨: int GetFileSize(FILE *fd)		 */
/*           fd:ÒÔ´ò¿ªµÄÎÄ¼þ                     */
/*           ·µ»Ø: -1 Ê§°Ü                       */
/*                 >=0 ÎÄ¼þ³¤¶È¢                 */
/*************************************************/
static int GetFileSize(fd)
int fd;
{
struct stat stat_s;
	if(fd==0)return -1;
	if(fstat(fd,&stat_s)) return -1;
	if(!(stat_s.st_mode & S_IFREG)) return -2;
	return stat_s.st_size;
}
int Z_GetFile(T_Connect *connect,T_NetHead *NetHead)
{
char buffer[SDBC_BLKSZ],fname[512],mname[512];
char *envp;
int i,size,e_node,repeat=0;
int fd;
int seekflag=0;
MD5_CTX md5ctx;
char md5[16];
int moveflg=0;
int Event_no=0;
    e_node=LocalAddr(connect->Socket,0);
    Event_no=NetHead->PROTO_NUM;
again:
    moveflg=NetHead->ERRNO2;
    substitute_env(fname);
    seekflag=NetHead->PKG_REC_NUM;
    strcpy(mname,NetHead->data);
    envp=getenv("SENDTMP");
    if(envp &&*envp) {
	sprintf(fname,"%s/%s.%d",envp,sc_basename(NetHead->data),getpid());
    } else {
	sprintf(fname,"/tmp/%s.%d",sc_basename(NetHead->data),getpid());
    }
    sprintf(buffer,"cp -f %s %s >/dev/null",mname,fname);
    i=system(buffer);
    if(i) {
	ShowLog(1,"syserr=%04X:%s",buffer);
	NetHead->ERRNO1=i;
	NetHead->data=buffer;
	NetHead->PKG_LEN=strlen(NetHead->data)+1;
	goto errret;
    }
    fd=open(fname,O_RDONLY);
    if(fd<0){
	ShowLog(1,"Can Not Open File:%s errno:%d:%s",
			fname,errno,
			strerror_r(errno,buffer,sizeof(buffer)));
	unlink(fname);
     	NetHead->ERRNO1=errno;
     	NetHead->PKG_LEN=0;
	NetHead->data=0;
errret:
     	NetHead->PROTO_NUM=PutEvent(connect,Event_no);
     	NetHead->ERRNO2=0;
     	NetHead->D_NODE=0;
     	NetHead->O_NODE=e_node;
     	NetHead->PKG_REC_NUM=0;
	i=SendPack(connect,NetHead);
	return 0;
    } 
    {
	int r_len=0;
    	size=GetFileSize(fd);
	MD5_Init(&md5ctx);
	NetHead->PKG_LEN=0;
     	if(size>0) {
		NetHead->ERRNO2=PACK_CONTINUE;
     		NetHead->ERRNO1=0;
     		NetHead->PROTO_NUM=-1;

	} else {
		NetHead->ERRNO2=0;
     		NetHead->ERRNO1=EINVAL;
     		NetHead->PROTO_NUM=0;
	     	NetHead->O_NODE=e_node;
	}
	NetHead->PROTO_NUM=0;
     	NetHead->PKG_REC_NUM=size;
     	NetHead->O_NODE=e_node;
     	NetHead->D_NODE=0;
	i=SendPack(connect,NetHead);
	if(i || size<=0){
		ShowLog(1,"Z_GetFile SendPack err %d,size=%d",i,size);
		close(fd);
		unlink(fname);
		return 1;
	}
     	NetHead->ERRNO1=0; 
     	NetHead->O_NODE=e_node;
     	NetHead->D_NODE=0;
	if(seekflag)lseek(fd,seekflag,SEEK_SET);
	do {
		*buffer=0;
		r_len=read(fd,buffer,sizeof(buffer));
		if(r_len<=0)break;
		MD5_Update(&md5ctx,buffer,r_len);
     		NetHead->PROTO_NUM=0;
     		NetHead->PKG_REC_NUM=r_len;
     		NetHead->PKG_LEN=r_len;
		NetHead->data=buffer;
		NetHead->ERRNO2= PACK_CONTINUE;
		i=SendPack(connect,NetHead);
		if(i<0){
			ShowLog(1,"Z_GetFile SendPack2 err %d",i);
			close(fd);
			unlink(fname);
			return -1;
		}
	} while(r_len==sizeof(buffer));
	close(fd);
	unlink(fname);
	MD5_Final((unsigned char *)md5,&md5ctx);
     	NetHead->PROTO_NUM=PutEvent(connect,Event_no);
	NetHead->data=md5;
     	NetHead->PKG_LEN=16;
	NetHead->ERRNO1= 0;
	NetHead->ERRNO2= 0;
	i=SendPack(connect,NetHead);
	i=RecvPack(connect,NetHead);
	if(i<0) {
		ShowLog(1,"Z_GetFile %s final net error=%d",mname,i);
		return -1;
	}
	if(NetHead->ERRNO1==CRCERR) {
		ShowLog(1,"Z_GetFile Repeat %d",++repeat);
		i=RecvPack(connect,NetHead);
		goto again;
	} else if(NetHead->ERRNO1) {
		ShowLog(1,"Z_GetFile %s Fail error %d",mname,NetHead->ERRNO1);
		return(0);
	}
    }
    ShowLog(2,"Z_GetFile_Msg %s success",mname);   
// move file to bak
    if(moveflg>0) {
	envp=getenv("SENDBAK");
	if(envp && *envp) {
		sprintf(buffer,"mv -f %s %s/%s >/dev/null",
			mname,envp,sc_basename(mname));
		if(!(i=system(buffer))) {
			ShowLog(2,"%s",buffer);
		} else {
			ShowLog(1,"%s ret=%04X",buffer,i);
		}
	}
    }
    return 0;
}
int Z_PutFile(T_Connect *connect,T_NetHead *NetHead)
{
char fname[512],mname[512],buffer[512],*cp;
int flg,i,count,e_node;
int fd;
int breakflag;
MD5_CTX md5ctx;
char md5[16];
int repeat=0;
int Event_no=0;
	e_node=LocalAddr(connect->Socket,0);
	Event_no=NetHead->PROTO_NUM;
again:
	if(!NetHead->PKG_LEN) {
		ShowLog(1,"Z_PutFile Filename EMPTY!");
		NetHead->ERRNO1=EINVAL;
		goto errret;
	}
    breakflag=NetHead->PKG_REC_NUM;
/*
    CharType[0].len=NetHead->PKG_LEN;
    net_dispack(buffer,NetHead->data,CharType);
*/
	strcpy(fname,NetHead->data);
	if(*fname=='$') {
		if(fname[1]=='/') strsubst(fname+1,0,"RECVDIR"); // $/filename
	} else if(!strncmp(fname,"@/",2)) strsubst(fname,1,"$HOME");
	substitute_env(fname);
    ShowLog(5,"Z_PutFile %s",fname);
	fd=0;
	cp=getenv("RECVTMP");
	if(cp && *cp) {
		sprintf(mname,"%s/%s.%d",cp,sc_basename(fname),getpid());
	} else {
		*mname=0;	
	}
/* ¶ÏµãÐø´«ÓÃ£¬»¹Ã»µ÷Í¨¡£
    if(breakflag){
	fd=open(fname,O_WRONLY|O_APPEND|O_TRUNC,S_IREAD|S_IWRITE);
	if(!fd)
	    fd=open(fname,O_WRONLY|O_CREAT|O_TRUNC,S_IREAD|S_IWRITE);
    } else  
*/
	fd=open(*mname?mname:fname,O_WRONLY|O_CREAT|O_TRUNC,
			S_IREAD|S_IWRITE| S_IRGRP| S_IROTH);//in windows,bin
    if(fd<0){
     	NetHead->ERRNO1=errno;
	ShowLog(1,"Z_PutFile:Can't Open File:%s errno:%d,%s",
			*mname?mname:fname,errno,strerror(errno));
errret:
     	NetHead->PROTO_NUM=PutEvent(connect,Event_no);
	NetHead->data=0;
	NetHead->PKG_LEN=0;
     	NetHead->PKG_REC_NUM=0;
     	NetHead->ERRNO2=0; 
     	NetHead->D_NODE=0;
     	NetHead->O_NODE=e_node;
	i=SendPack(connect,NetHead);
	return 1;
    } 
    if(breakflag){
	lseek(fd,0,SEEK_END);
    	NetHead->PKG_REC_NUM = GetFileSize(fd);
    }
    else NetHead->PKG_REC_NUM=0;
     	NetHead->PROTO_NUM=0;
	NetHead->ERRNO1=0;
	NetHead->ERRNO2=0; 
     	NetHead->O_NODE=e_node;
	NetHead->D_NODE=0;
	NetHead->data=0;
	NetHead->PKG_LEN= 0;
	i=SendPack(connect,NetHead);
    
	count=1;
	flg=0;
	MD5_Init(&md5ctx);
	do {
		i=RecvPack(connect,NetHead);
		if(i){
			ShowLog(1,"Z_PutFile Recv:%s errno:%d",NetHead->data,i);
			NetHead->ERRNO1=errno;
			close(fd);
			if(*mname) unlink(mname);
			goto errret;
		}
		if(NetHead->ERRNO2 != PACK_CONTINUE) break;
		if(NetHead->PKG_LEN&&!flg){
			i= NetHead->PKG_LEN;
			MD5_Update(&md5ctx,NetHead->data,i);
			i=write(fd,NetHead->data,NetHead->PKG_LEN);
			if(i<0) {
				flg=errno;
				ShowLog(1,"Z_PutFile:Write %d File:%s count=%d i=%d,errno:%d,%s",
					count,fname,count,i,errno,
					strerror_r(errno,buffer,512));
				break;
			}
			count++;
		}
	} while(NetHead->ERRNO2 == PACK_CONTINUE);
	close(fd);
	if(flg) {
		NetHead->ERRNO1=flg;
		if(*mname) unlink(mname);
		goto errret;
	}

	NetHead->ERRNO1=0;
	MD5_Final((unsigned char *)md5,&md5ctx);
	if(memcmp(md5,NetHead->data,16)) {
	    if(*mname) unlink(mname);
	    if(++repeat <= REPEAT) {
		ShowLog(1,"Z_PutFile %s Fail Repeat %d",fname,repeat);
		NetHead->ERRNO1=CRCERR;
		NetHead->ERRNO2=0;
	     	NetHead->PKG_REC_NUM=0;
     		NetHead->O_NODE=e_node;
     		NetHead->D_NODE=0;
     		NetHead->PKG_LEN=0;
		NetHead->data=0;
		i=SendPack(connect,NetHead);
		i=RecvPack(connect,NetHead);
		if(i<0) {
			ShowLog(1,"Z_PutFile %s Fail Network error %d",fname,i);
			return(-1);
		} else goto again;
	    } else {
			ShowLog(1,"Z_PutFile %s Fail err=%d",
					fname,NetHead->ERRNO1);
			NetHead->ERRNO1=-1;
	    }
	} else {
		if(*mname) {
			sprintf(buffer,"mv -f %s %s >/dev/null",mname,fname);
			i=system(buffer);
			if(i) {
				ShowLog(1,"%s,error=%04X",buffer,i);
				unlink(mname);
				NetHead->ERRNO1=i;
			}
		}
	}
	
     	NetHead->PROTO_NUM=PutEvent(connect,Event_no);
	NetHead->ERRNO2=0;
     	NetHead->PKG_REC_NUM=0;
     	NetHead->O_NODE=LocalAddr(connect->Socket,0);
     	NetHead->D_NODE=0;
	NetHead->data=0;
     	NetHead->PKG_LEN=0;
	i=SendPack(connect,NetHead);
	ShowLog(2,"Z_PutFile %s succes",fname);
    return 0;
}

