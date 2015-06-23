#include <arpa/inet.h>
#include <sc.h>

int net_showlog(T_Connect *conn,T_NetHead *NetHead)
{
char *old_showid;
char new_showid[200],*p,cliaddr[16];

	if(NetHead->ERRNO2 != PACK_NOANSER)
		ShowLog(1,"net_showlog ERRNO2 Must be PACK_NOANSER!");
	if(NetHead->PKG_REC_NUM<=0)
		ShowLog(NetHead->ERRNO1,"net_showlog:%s",NetHead->data);
	else {
		old_showid=Showid;
		p=stptok(NetHead->data,new_showid,sizeof(new_showid),"|");
		if(*p=='|') {
		char *p2;

			p++;
			StrAddr(htonl(NetHead->O_NODE),cliaddr);
			p2=strsubst(new_showid,0,cliaddr);
			strins(p2,':');
			Showid=new_showid;
			ShowLog(NetHead->ERRNO1,"%s",p);
		}
		Showid=old_showid;
	}
	return 0;
}
