#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "midware.h"

#define MIN(a,b) ((a) < (b))?(a):(b)
extern void quit(int n);

#define longaddr(s) LongAddr(s)
//从服务器收包发往客户端
static int from_server(T_Connect *client,T_NetHead *NetHead)
{
T_SRV_Var *srvp=(T_SRV_Var *)client->Var;
GDA *gp=(GDA *)srvp->var;
int i;
char tmp[256];
	i=get_event_status(srvp->TCB_no);
	if(i!=1) { //EPOLLIN
		sprintf(tmp,"呼叫服务器返回超时");
		NetHead->ERRNO2=-1;
		goto err;
	}
	i=RecvPack(&gp->server,NetHead);
	if(i<0){
		sprintf(tmp,"recv from server %s fail i=%d,errno=%d,%s",
			gp->server.Host,i,errno,strerror(errno));
		NetHead->ERRNO2=i;
err:
		NetHead->ERRNO1=errno;
 	   	ShowLog(1,"%s:%s",__FUNCTION__,tmp);
		NetHead->data=tmp;
		NetHead->PKG_LEN=strlen(NetHead->data);
		NetHead->O_NODE=LocalAddr(client->Socket,0);
		i=SendPack(client,NetHead);
		clr_event(srvp->TCB_no);
		return -1;
	} 
	gp->t_cont=NetHead->ERRNO2;
	ShowLog(2,"Recv from server %s PROTO_NUM=0X%04X,ERRNO1=%d,ERRNO2=%d,PKG_LEN=%d",
		gp->server.Host,NetHead->PROTO_NUM, NetHead->ERRNO1,
		NetHead->ERRNO2, NetHead->PKG_LEN);

	i=SendPack(client,NetHead);
//ShowLog(5,"SendPack to client i=%d",i);

	if(gp->t_cont!=PACK_CONTINUE) 
		clr_event(srvp->TCB_no);
//如果服务器要求连续传送，就不清除事件，后续继续由这个函数处理。		
	return 0;
}
//从客户端收包，发往服务器
int Transfer(T_Connect *client,T_NetHead *NetHead)
{
T_SRV_Var *srvp=(T_SRV_Var *)client->Var;
GDA *gp=(GDA *)srvp->var;
int i;
T_Connect *server; 
char localaddr[16];
char tmp[256];
int proto_num=NetHead->PROTO_NUM;
	server=&gp->server;;
	LocalAddr(client->Socket,localaddr);
/* if data is'nt ascii, bug
	ShowLog(5,"send to srv len:%d,SeneLen=%d,RecvLen=%d,data=%s",
		NetHead->PKG_LEN,client->SendLen,client->RecvLen,NetHead->data);
*/
	gp->t_cont=NetHead->ERRNO2;

	i=SendPack(server,NetHead);
ShowLog(4,"Transfer to serv proto_num=0X%04X,t_cont=%d,SendPack ret=%d", proto_num,gp->t_cont,i);
	if(i){
		NetHead->ERRNO1=errno;
		NetHead->ERRNO2=i;
		sprintf(tmp,"Send to %s fail ret=%d,errno=%d,%s",
			server->Host,i,errno,strerror(errno));
errret:
 	   	ShowLog(1,"transfer %s",tmp);
		NetHead->O_NODE=longaddr(localaddr);
		NetHead->data=tmp;
		NetHead->PKG_LEN=strlen(NetHead->data)+1;
		i=SendPack(client,NetHead);
		return -1;
	} 
	while(gp->t_cont==PACK_CONTINUE) {
    		i=RecvPack(client,NetHead);
		if(i) {
			sprintf(tmp,"recv from client ret=%d,errno=%d,%s",
				i,errno,strerror(errno));
			return -1;
		}
//ShowLog(5,"Transfer c->s CONTINUE PROTO_NUM=%04X",NetHead->PROTO_NUM);
		gp->t_cont=NetHead->ERRNO2;
//ShowLog(5,"Transfer CONTINUE data=%s",NetHead->data);
    		i=SendPack(server,NetHead);
		if(i) {
			NetHead->ERRNO1=errno;
			NetHead->ERRNO2=i;
			sprintf(tmp,"Send to %s fail ret=%d,errno=%d,%s",
				server->Host,i,errno,strerror(errno));
			goto errret;
		}
	}
	if(gp->t_cont==PACK_NOANSER) {
ShowLog(4,"Transfer 2:PACK_NOANSER");
		return 0;
	}
//
	set_event(srvp->TCB_no,server->Socket,from_server);
	return 0;
}

