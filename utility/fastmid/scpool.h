#ifdef __cplusplus

extern "C" {
#endif

//scpool.c
int scpool_init(void);
void scpool_free(void);
void scpool_check(void);

T_Connect * get_SC_connect(int TCBno,int n,int flg);
void release_SC_connect(T_Connect **Connect,int TCBno,int poolno);
int get_scpool_no(int d_node);
int get_scpoolnum(void);
char *get_LABEL(int poolno);
int conn_no(int poolno,T_Connect *conn);
int  conn_lock(int poolno,int conn_no);
int  conn_unlock(int poolno,int conn_no);

//mod_sc.c
void wpool_free(void);
int get_s_connect(int TCBno,int poolno,T_Connect **connp,int (*call_back)(T_Connect *,T_NetHead *));
//fastmid.c
int bind_sc(int TCBno,T_Connect *conn);
int unbind_sc(int TCBno);
/**
 * 归还连接到流水线供其他任务共享  
 */
int share_lnk(int poolno,T_Connect *conn);

#ifdef __cplusplus
}
#endif
