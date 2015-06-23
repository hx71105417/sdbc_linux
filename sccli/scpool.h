#include <sccli.h>

typedef struct {
	int TCBno;
	T_Connect Conn;
	T_CLI_Var cli;
	int pool_no;
	INT64 timestamp;
} resource;

extern char diagTrip_time[30];
#ifdef __cplusplus
extern "C" {
#endif

//scpool.c
int scpool_init(void);
void scpool_free(void);
void scpool_check(void);

resource * get_SC_resource(int TCBno,int n,int flg);
void release_SC_resource(resource **rsp);
T_Connect * get_SC_connect(int TCBno,int n,int flg);
void release_SC_connect(T_Connect **Connect,int TCBno,int poolno);

int get_scpool_no(int d_node);
int get_scpoolnum(void);
char *get_LABEL(int poolno);
//负载均衡，找一个负载最轻的池
resource * get_SC_weight(int TCBno);

#ifdef __cplusplus
}
#endif

