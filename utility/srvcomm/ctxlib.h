/***************************************************
 * 远程认证程序 SDBC 7.0
 * 适用于单机，单服务器的远程认证
 * 多机的认证信息应记录在数据库里
 * *************************************************/

#include <pack.h>
#include <ctx.stu>

#ifdef __cplusplus
extern "C" {
#endif

void ctx_check(void);
CTX_stu * get_ctx(int ctx_id);
int set_ctx(CTX_stu *ctxp);
int ctx_del(CTX_stu *ctxp);
void ctx_free(void);

#ifdef __cplusplus
}
#endif
