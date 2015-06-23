/* 输入数据18d时所用的校验程序 check.c  */
#include <stdio.h>
#include <math.h>
#include "calc.h"
#include "srw.h"

extern int errno;
extern struct srw_s Table;
struct srw_s *srwp=&Table;
double chkdata(int,int,char *[CALCID]);

char *stptok();
int row;          /* 当前行 */

/*    校验结果   1:正确   0:错误    */

/* 表内校验程序的主程序 check.c  */
check(jfd)    
FILE *jfd;
{
int n,cc,err=0;
	/*wclear(wp);   清校验区 */
	err=dispchk(jfd,srwp);
	return(err);
}

/*  读字典,屏幕提示?   */
dispchk(jfd)
FILE *jfd;
{
char strj[256];
int cc;
int err;
double x,y;
	err=1;
	while(fgets(strj,sizeof(strj),jfd)) {
		if(feof(jfd)) break;
		TRIM(strj);
		if(*strj=='#' || *strj==';' || !*strj) continue;
		if(!chkstr(strj,&x,&y,chkdata)) {
			err=0;
/* 凡校验不成功的行(err=0)将校验字典的该行及计算后的左右值在校验行提示 */
			printf("%lg(%lg).........%s\n",x,y,strj); 
		}
	}
	return(err);
}

double chkdata(n,argc,argv)
int n,argc;
char *argv[CALCID];
{
	if(argc==1) {
		s(3,argv[1]);
	}
	else if(argc==2) {
		s(1,argv[1]);
		s(3,argv[2]);
	}
	return(readitem(srwp,n));
}
