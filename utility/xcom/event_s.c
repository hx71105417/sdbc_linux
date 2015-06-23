#define SERVER
#include "xcomd.h"
//Eno：事件号，一般是协议号 
int event_put(T_Connect *conn,int Eno)
{
	ShowLog(5,"enevt_put Eno=%d",Eno);
	return Eno;
}
