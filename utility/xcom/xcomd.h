#include <sdbc.h>
#include <sccli.h>
int event_put(T_Connect *conn,int Eno);
int Z_GetFile(T_Connect *connect,T_NetHead *NetHead);
int Z_PutFile(T_Connect *connect,T_NetHead *NetHead);

typedef struct {
	char Showid[100];
} GDA;
