/***************************************************
 * 远程认证程序 SDBC 7.0
 * 适用于单机，单服务器的远程认证
 * 这里仅为TPC、TPOOL模式管理一个内存数据库.
 * 多机的认证信息应记录在数据库里
 * *************************************************/

#include <ctxlib.h>
#include <BB_tree.h>
#include <crc32.h>

T_PkgType CTX_tpl[]={
	{CH_INT4,sizeof(INT4),"ctx_id",0,-1},
        {CH_CHAR,17,"DEVID"},
	{CH_TIME,sizeof(INT64),"usetime",YEAR_TO_SEC},
	{-1,0,0,0}
};

static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
static T_Tree *ctxlib=NULL;

static int ctx_cmp(void *s,void *d,int len)
{
//int ret;
	if(len==sizeof(CTX_stu)) {
		return ((CTX_stu *)s)->ctx_id == ((CTX_stu *)d)->ctx_id?0:
			((CTX_stu *)s)->ctx_id > ((CTX_stu *)d)->ctx_id?1:-1;
			
//ShowLog(5,"%s:s=%d,d=%d,ret=%d",__FUNCTION__,((CTX_stu *)s)->ctx_id, ((CTX_stu *)d)->ctx_id,ret);
	} else return (*(CTX_stu **)s)->ctx_id == (*(CTX_stu **)d)->ctx_id?0:
		(*(CTX_stu **)s)->ctx_id > (*(CTX_stu **)d)->ctx_id?1:-1;
}

typedef struct {
	T_Tree **del_tree;
	INT64 now;
}del_node;

static int del_count(T_Tree *sp,void *d)
{
CTX_stu *ctxp=(CTX_stu *)sp->Content;
del_node *dp=(del_node *)d;
	if(3600<(dp->now - ctxp->usetime)) {//超过1小时
		*dp->del_tree=BB_Tree_Add(*dp->del_tree,&ctxp,sizeof(CTX_stu *),ctx_cmp,NULL);
		return 1;
	}
	return 0;
}

static int ctx_count(T_Tree *sp,void *d)
{
CTX_stu *ctxp=(CTX_stu *)sp->Content;
CTX_stu *dp=(CTX_stu *)d;
	if(!strcmp(ctxp->DEVID,dp->DEVID)) {
		return 1;
	} else return 0;
}

static void del_ctxlib(void *s)
{
int flg=0;
CTX_stu *sp=*(CTX_stu **)s;
	ctxlib=BB_Tree_Del(ctxlib,sp,sizeof(CTX_stu),ctx_cmp,NULL,&flg);
	ShowLog(5,"%s:delete %s,%u,%d",__FUNCTION__,sp->DEVID,sp->ctx_id,flg);
}

void ctx_check(void)
{
T_Tree *del=NULL;
del_node delctx;
int count;
	delctx.del_tree=&del;
	delctx.now=now_sec();
//把符合删除条件的记录收集到del
	count=BB_Tree_Count(ctxlib,&delctx,del_count);
	if(count>0) {
		pthread_rwlock_wrlock(&rwlock);
//删除这些记录
		BB_Tree_Free(&del,del_ctxlib);
		pthread_rwlock_unlock(&rwlock);
		ShowLog(2,"%s:free %d CTX's",__FUNCTION__,count);
	}
}

CTX_stu * get_ctx(int ctx_id)
{
T_Tree *tp;
CTX_stu key;
	if(!ctx_id) return NULL;
	key.ctx_id=ctx_id;
	pthread_rwlock_rdlock(&rwlock);
	tp=BB_Tree_Find(ctxlib,&key,sizeof(key),ctx_cmp);
	pthread_rwlock_unlock(&rwlock);
	if(!tp) {
		return NULL;
	}
	((CTX_stu *)tp->Content)->usetime=now_sec();
	return (CTX_stu *)tp->Content;
}

int set_ctx(CTX_stu *ctxp)
{
T_Tree *tp;
int ret;
	pthread_rwlock_wrlock(&rwlock);
	do {
		ctxp->usetime=now_usec();
		ctxp->ctx_id=(INT4)ssh_crc32((unsigned char *)&ctxp->usetime,sizeof(ctxp->usetime));
		if(ctxp->ctx_id==-1) continue;
		tp=BB_Tree_Find(ctxlib,ctxp,sizeof(CTX_stu),ctx_cmp);
	} while (tp);
	ctxp->usetime/=1000000;
	ctxlib=BB_Tree_Add(ctxlib,ctxp,sizeof(CTX_stu),ctx_cmp,NULL);
	ret=BB_Tree_Count(ctxlib,ctxp,ctx_count);
	pthread_rwlock_unlock(&rwlock);
	return ret;
}

void ctx_free(void)
{
	pthread_rwlock_wrlock(&rwlock);
	BB_Tree_Free(&ctxlib,NULL);
	pthread_rwlock_unlock(&rwlock);
}

int ctx_del(CTX_stu *ctxp)
{
int flg=0;
	pthread_rwlock_wrlock(&rwlock);
	ctxlib=BB_Tree_Del(ctxlib,ctxp,sizeof(CTX_stu),ctx_cmp,NULL,&flg);
	pthread_rwlock_unlock(&rwlock);
	return flg;
}
