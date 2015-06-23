#include <sdbc.h>
#include <unistd.h>
#include <sys/utsname.h>

int clilogin(T_Connect *conn,T_NetHead *NetHead)
{

int cc;
char *p,*p1;
struct utsname ubuf;
char buf[256],pack[240]; // ca leng?
	cc=uname(&ubuf);
	p1=getlogin();
	LocalAddr(conn->Socket,pack);
	*buf=0;
	sprintf(buf,"%s|%s|%s.%s.%s|",getenv("DEVID"),getenv("LABEL"),
		ubuf.nodename,p1,pack);
//printf("logbuf=%s\n",buf);
	p=getenv("SENDSIZE");
        if(p && *p) {
                conn->MTU=atoi(p);
        }

	NetHead->PROTO_NUM=0;
	NetHead->data=buf;
	NetHead->PKG_LEN=strlen(NetHead->data);
	NetHead->ERRNO1=0;
	NetHead->ERRNO2=conn->MTU;
	NetHead->D_NODE=0;
	NetHead->O_NODE=LocalAddr(conn->Socket,0);
	p=getenv("DNODE");
	if(p&&*p) NetHead->D_NODE=atoi(p);
//printf("D_NODE=%d",NetHead->D_NODE);
	cc=SendPack(conn,NetHead);
	if(cc) {
		ShowLog(0,"send %d\n",cc);
		return cc;
	}
	cc=RecvPack(conn,NetHead);
	
	return cc;
}
