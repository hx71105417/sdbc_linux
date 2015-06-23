
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#define CLIENT
#include <md5.h>
#include "xcom.h"

#define REPEAT 3

static void showhex(int level,char *title,char *str,int i)
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

static int N_Get_File_Size(fd)
int fd;
{
struct stat stat_s;
        if(fd==0)return -1;
        if(fstat(fd,&stat_s))return -1;
        return stat_s.st_size;
}


int NZ_Put_File(T_Connect *connect,char *local_file,char *remote_file)
{
        return NZ_Put_File_Msg(connect,local_file,remote_file,0);
}
int NZ_Put_File_Msg(T_Connect *connect,
	char *local_file,
	char *remote_file,
	void (*Msg)(int,int))
{
int i,r_len;
int fd;
char buffer[SDBC_BLKSZ];//sendbuf[SDBC_BLKSZ];
T_NetHead NetHead;
unsigned long  FileSize,count=0;
INT4 O_NODE;
MD5_CTX md5ctx;
unsigned char md5[16];
int Event_no=0;
	O_NODE=LocalAddr(connect->Socket,0);
again:
    fd=open(local_file,O_RDONLY);
    if(fd<0){
	return errno;
    }
    FileSize=N_Get_File_Size(fd);
    NetHead.ERRNO2=1;
    NetHead.PKG_REC_NUM=FileSize;
    NetHead.ERRNO1=0;
    NetHead.PROTO_NUM=get_srv_no(connect->Var,"Z_PutFile");
    NetHead.O_NODE=O_NODE;
    NetHead.D_NODE=0;
    NetHead.data=remote_file;
    NetHead.PKG_LEN=strlen(NetHead.data);
//ShowLog(5,"N_Put_File local_file=%s,data=%s",local_file,NetHead.data);
    i=SendPack(connect,&NetHead);
    i=RecvPack(connect,&NetHead);
    if(i){
	ShowLog(1,"PutFile recv net err ret=%d,errno=%d",i,errno);
	return i;
    }
    Event_no=NetHead.PROTO_NUM;
    if(NetHead.ERRNO1) {
	EventCatch(connect,NetHead.PROTO_NUM);
	StrAddr(NetHead.O_NODE,buffer);
	ShowLog(1,"NZ_PutFile_Msg recv err errno1=%d,errno2=%d,enode=%s",
		NetHead.ERRNO1,NetHead.ERRNO2,buffer);
	return(NetHead.ERRNO1);
    }
//ShowLog(5,"N_Put_File errno1=%d,errno2=%d continue %s",
//		NetHead.ERRNO1,NetHead.ERRNO2,NetHead.data);
    MD5_Init(&md5ctx);
    {
     	NetHead.ERRNO2=PACK_CONTINUE;
     	NetHead.PKG_REC_NUM=0;
     	NetHead.ERRNO1=0;
     	NetHead.PROTO_NUM=get_srv_no(connect->Var,"Z_PutFile");
     	NetHead.O_NODE=O_NODE;
     	NetHead.D_NODE=0;
	do {
		r_len=read(fd,buffer,sizeof(buffer));
		if(r_len<=0)break;
		MD5_Update(&md5ctx,buffer,r_len);
     		NetHead.PKG_REC_NUM=0;
     		NetHead.PKG_LEN=r_len;
		NetHead.data=buffer;
		i=SendPack(connect,&NetHead);
		if(i){
			ShowLog(1,"PutFile loop send ret=%d,errno=%d",1,errno);
			close(fd);
			return i;
		}
                count+=r_len;
                if(Msg)Msg(FileSize,count);
	} while(r_len==sizeof(buffer));
	MD5_Final(md5,&md5ctx);
     	NetHead.PKG_LEN=16;
	NetHead.data=(char *)md5;
	NetHead.ERRNO2=0;
	NetHead.ERRNO1=0;
	i=SendPack(connect,&NetHead);
	close(fd);
    }
    i=RecvPack(connect,&NetHead);
    if(i)return i;
	EventCatch(connect,NetHead.PROTO_NUM);
    if(NetHead.ERRNO1==CRCERR) {
	ShowLog(1,"NZ_PutFile_Msg %s Repeat",remote_file);
	goto again;
    } else if(NetHead.ERRNO1) {
	   StrAddr(NetHead.O_NODE,buffer);
	   ShowLog(1,"NZ_PutFile_Msg Final recv errno1=%d,errno2=%d,enode=%s",
		NetHead.ERRNO1,NetHead.ERRNO2,buffer);
    }
    EventCatch(connect,Event_no);
    return NetHead.ERRNO1;
}

int NZ_Get_File(T_Connect *connect,char *local_file,char *remote_file)
{
	return NZ_Get_File_Msg(connect,local_file,remote_file,0);

}

int NZ_Get_File_Msg(T_Connect *connect,
	char *local_file,
	char *remote_file,
	void (*Msg)(int,int))
{
char *envp;
//char filebuf[SDBC_BLKSZ];
int i;
int fd;
T_NetHead NetHead;
unsigned long FileSize=0,count;
INT4 O_NODE;
MD5_CTX md5ctx;
unsigned char md5[16];
int repeat=0;
int Event_no=0;
	O_NODE=LocalAddr(connect->Socket,0);
    ShowLog(5,"文件传输 本地文件名:%s <-------- 远程文件名:%s",
			local_file,remote_file);
again:
	count=0;
    NetHead.ERRNO2=0;
	envp=getenv("MVFLG");
	if(envp && *envp) NetHead.ERRNO2=atoi(envp);
    NetHead.PKG_REC_NUM=0;
    NetHead.ERRNO1=0;
    NetHead.PROTO_NUM=get_srv_no(connect->Var,"Z_GetFile");
    NetHead.O_NODE=O_NODE;
    NetHead.D_NODE=0;
    NetHead.data=remote_file;
    NetHead.PKG_LEN=strlen(NetHead.data)+1;
    i=SendPack(connect,&NetHead);
    i=RecvPack(connect,&NetHead);
    if(i) {
	ShowLog(1,"NZ_PutFile_Msg:RecvPack err=%d",i);
    	return i;
    }
    Event_no=NetHead.PROTO_NUM;
    if(NetHead.ERRNO1) {
	ShowLog(1,"NZ_PutFile_Msg:ERRNO1=%d,%s",NetHead.ERRNO1,NetHead.data);
    	return NetHead.ERRNO1;
    }
    fd=open(local_file,O_WRONLY|O_CREAT|O_TRUNC,S_IREAD|S_IWRITE|S_IRGRP|S_IROTH);
    if(fd<0){
/* 打开文件失败 还是要传，因为那边已经不管这些了 */
	ShowLog(1,"Open %s err %d,%s",local_file,errno,strerror(errno));
    }
    if(NetHead.PKG_REC_NUM>0){
                FileSize=NetHead.PKG_REC_NUM;
    }
	MD5_Init(&md5ctx);
    while(NetHead.ERRNO2==PACK_CONTINUE) {
	i=RecvPack(connect,&NetHead);
    	if(i){
    		close(fd);
		ShowLog(1,"NZ_PutFile_Msg:RecvPack err=%d",i);
    		return i;
    	}
	if(NetHead.PROTO_NUM) {
		Event_no=NetHead.PROTO_NUM;
		ShowLog(5,"NZ_GetFile Event_no=%d",Event_no);
	}
	if(NetHead.ERRNO2 != PACK_CONTINUE) break;
    	if(NetHead.PKG_LEN){
		MD5_Update(&md5ctx,NetHead.data,NetHead.PKG_LEN);
                count+=NetHead.PKG_LEN;
	  if(fd>=0) {
    		i=write(fd,NetHead.data,NetHead.PKG_LEN);
                if(i!=NetHead.PKG_LEN){
			ShowLog(1,"NZ_PutFile_Msg %s errno-%d,%s",local_file,
					errno,strerror(errno));
			
			i=errno;
                        close(fd);
			NetHead.ERRNO1=i;
			NetHead.ERRNO2=0;
			NetHead.PROTO_NUM=get_srv_no(connect->Var,"Z_GetFile");
			NetHead.data=0;
			NetHead.PKG_LEN=0;
			NetHead.PKG_REC_NUM=0;
			i=SendPack(connect,&NetHead);
                        return  NetHead.ERRNO1;
                }
                if(Msg)Msg(FileSize,count);
	  }
    	}
    }
    if(fd>=0) {
	close(fd);
	fd=0;
	NetHead.ERRNO1=0;
	MD5_Final(md5,&md5ctx);
	if(memcmp(md5,NetHead.data,16)) {
		NetHead.O_NODE=LocalAddr(connect->Socket,0);
		ShowLog(5,"NZ_GetFile_Msg %s:MD5 error",remote_file);
		showhex(5,"localmd5",(char *)md5,16);
		showhex(5,"NetHead.data",NetHead.data,16);
		if(++repeat <= REPEAT)  {
			NetHead.ERRNO1=CRCERR;
			NetHead.ERRNO2=0;
			NetHead.PROTO_NUM=get_srv_no(connect->Var,"Z_GetFile");
			NetHead.data=0;
			NetHead.PKG_LEN=0;
			NetHead.PKG_REC_NUM=0;
			i=SendPack(connect,&NetHead);
			goto again;
		}
		NetHead.ERRNO1=1;
	}
    } else {
	NetHead.ERRNO1=1;
    }

	NetHead.ERRNO2=PACK_NOANSER;
	NetHead.PROTO_NUM=get_srv_no(connect->Var,"Z_GetFile");
	NetHead.data=0;
	NetHead.PKG_LEN=0;
	NetHead.PKG_REC_NUM=0;
	i=SendPack(connect,&NetHead);
    EventCatch(connect,Event_no);
    return NetHead.ERRNO1;
}

