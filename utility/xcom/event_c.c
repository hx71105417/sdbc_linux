#define CLIENT
#include "xcom.h"
//客户端，抓取事件 
int event_catch(T_Connect *conn,int event_no)
{
	ShowLog(5,"event_catch event_no=%d\n",event_no);
	return event_no;
}
