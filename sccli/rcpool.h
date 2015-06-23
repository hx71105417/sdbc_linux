#ifdef __cplusplus

//一个适用于多线程环境的sdbc连接池，线程池环境的见../mod_sc
extern "C" {
#endif

//rcpool.c
int rcpool_init(void);
void rcpool_free(void);
void rcpool_check(void);

T_Connect * get_RC_connect(int poolno,int flg);
void release_RC_connect(T_Connect **Connect,int poolno);
int get_rcpool_no(int d_node);
int get_rcpoolnum(void);
char *get_rcLABEL(int poolno);

#ifdef __cplusplus
}
#endif
