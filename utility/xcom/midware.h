#include <sdbc.h>
#include <sccli.h>

typedef struct {
	T_Connect server;
	u_int newfamily[32];
	int t_cont;
	char Showid[100];
} GDA;

extern int Transfer(T_Connect *conn, T_NetHead *NetHead);

