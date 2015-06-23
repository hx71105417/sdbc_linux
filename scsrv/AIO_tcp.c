/**********************************************
 * @(#) SDBC 7.1 TCP SERVER Tools                      *
 * to suport Fiberized.IO
 * 重载 socket/tcp.c
 **********************************************/
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <scsrv.h>

#ifndef MIN
#define MIN(a,b) ((a)<(b))?(a):(b)
#endif

//timeout for second 
int RecvNet(int socket,char *buf,int n,int timeout)
{
int bcount=0,br,ret;
int i,repeat=0;
int fflag=-1;

	if(socket<0) return SYSERR;
	if(!buf && n<0) return 0;
	fflag=fcntl(socket,F_GETFL,0);
	if(fflag!=-1) fcntl(socket,F_SETFL,fflag|O_ASYNC|O_NONBLOCK); //异步操作

	*buf=0;
	br=0;

	while(bcount<n){
		if((br=read(socket,buf,n-bcount))>0){
			bcount+=br;
			buf+=br;
			repeat=0;
			continue;
		}
		if(fflag==-1 && errno==EAGAIN) return TIMEOUTERR;
		if(br<=0 && errno && errno != EAGAIN){
		    if(errno!=ECONNRESET)
			ShowLog(1,"%s:br=%d,err=%d,%s",__FUNCTION__,br,errno,strerror(errno));
		    break;
		}
		if(bcount < n && fflag!=-1) { //切换任务
			if(repeat++>3) return -errno;
//ShowLog(5,"%s:tid=%lX,socket=%d,yield to schedle bcount=%d/%d",__FUNCTION__,pthread_self(),socket,bcount,n);
			i=do_event(socket,0,timeout);//yield by EPOOLIN
			if(i<0) {
			  if(timeout>0) {
				struct timeval tmout;
				tmout.tv_sec=timeout;
        			tmout.tv_usec=0;
        			ret=setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,(char *)&tmout,sizeof(tmout));
			  }
				fcntl(socket,F_SETFL,fflag);
				fflag=-1;
				if(i==-11) return -11;
			}
		}
	}
	if(fflag!=-1) fcntl(socket,F_SETFL,fflag);
	return bcount==0?-1:bcount;
}

int SendNet(int socket,char *buf,int n,int MTU)
{
int bcount,bw;
int sz,i=0;
int fflag;
size_t SendSize;

	if(socket<0) return SYSERR;
	fflag=fcntl(socket,F_GETFL,0);
	if(fflag != -1) fcntl(socket,F_SETFL,fflag|O_NONBLOCK); //异步操作
	bcount=0;
	bw=0;
	if(MTU>500) SendSize=MTU;
	else SendSize=n;
	while(bcount<n){
		sz=MIN(n-bcount,SendSize);
		if((bw=write(socket,buf,sz))>0){
			bcount+=bw;
			buf+=bw;
		}
		if(bw<0&&errno!=EAGAIN) {
			ShowLog(1,"%s:err=%d,%s",__FUNCTION__,errno,strerror(errno));
			break;
		}
		if(bw < sz && fflag != -1) { //切换任务
ShowLog(5,"%s:tid=%lX,socket=%d,yield bw=%d/%d",__FUNCTION__,pthread_self(),socket,bw,sz);
		    i=do_event(socket,1,0); //yield by EPOLLOUT
		    if(i<0) {
			fcntl(socket,F_SETFL,fflag);
			fflag = -1;
		    }
		}
	}
	if(fflag != -1)  fcntl(socket,F_SETFL,fflag);
	return bcount==0?-1:bcount;
}

