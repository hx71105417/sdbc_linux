/*    getdata.c  取数  */
/*    chkstr.c-->calculator()-->getdata()-->srw() ??     */
#include "srw.h"
#include <stdio.h>
#include <time.h>
#include "calc.h"

double getdata(int n,int argc,char *argv[]);
double calprt(char *);/*计算表达式,按格式输出*/
extern double *l;
extern int labn;
extern FILE *ofd;
extern FILE *logfile;
extern char *strchr(),*strrchr();
extern long today;
extern struct srw_s Table; 

static char *cvt();
static void gb3qd(int);
static struct srw_s *srwp=&Table;

double getdata(n,argc,argv)
int n,argc;
char *argv[CALCID];
{
	int i,cc;
	double t;
	short ymd[3];




	srwp->rp=NULL;
	if (argc==0&&*argv[0]) {  
		switch(*argv[0]) {
		case 'L':  /* 处理标号表达式*/
			if(l && n>=0 && n<labn) {

				return(l[n]);
			}
			else  return(0.);
		case 'D': /* func in cvtdate.c */
			if(n==0) t=(double)mday(today);		
	
			else if(n==1) {    /* month end day */
				t=(double)mday((time_t)readitem(srwp,-srwp->skip));
			}
			else if(n==2) {    /* day of mon */
				t=(double)dday((time_t)readitem(srwp,-srwp->skip));
			}
			else if(n==3) {   /* day of year */
				t=(double)yday((time_t)readitem(srwp,-srwp->skip));
			}
			else if(n==4) { /* mon of today */
				rjulymd(today, ymd);
				t=(double)ymd[1];
			}
			else if(n==5) { /* mon of rec */
				rjulymd((time_t)
					readitem(srwp,-srwp->skip),ymd);
				t=(double)ymd[1];
			}
			else if(n==6) t=(double)today;
			else if(n==7)
				t=(double)jday((time_t)readitem(srwp,-srwp->skip));
/* year of rec */
			srwp->rp=0;
			return(t);
		default: return(0.);
		}
	} else if (*argv[0] == 'D' && argc == 1) { 
		switch(n) {
		case 0:  /* day of mon end */
			 t=(double)mday(cvtdate(argv[1],today));		
			break;
		case 1:
			t=(double)mday(cvtdate(argv[1],
					(time_t)readitem(srwp,-srwp->skip)));
			break;
		case 2:     /* day of mon */
			t=(double)dday(cvtdate(argv[1],today));
			break;
		case 3:    /* day of year */
			t=(double)yday(cvtdate(argv[1],today));
			break;
		case 4:  /*  mon of today */
			rjulymd(cvtdate(argv[1],today), ymd);
			t=(double)ymd[1];
			break;
		case 5:  
			rjulymd(cvtdate(argv[1],
				(time_t)readitem(srwp,-srwp->skip)), ymd);
			t=(double)ymd[1];
			break;
		case 6: t=(double)cvtdate(argv[1],today);
			break;
		case 7:    /* day of season */
			t=(double)jday(cvtdate(argv[1],today));
			break;
		default: return(0.);
		}
/*
fprintf(stderr,"getdata return %lg\n",t);
*/
		return(t);
	} else { /*填充索引场值*/
	   for(i=0;i<argc;i++) {

		if(*argv[i+1]) {
			if(*argv[i+1]=='@') { 
				s(srwp,i+1,cvt(argv[i+1]));
			}
			else {

				s(srwp,i+1,argv[i+1]);
			}
		}
	   }
	   switch(*argv[0]) {  
	   default:break;
	   }
	}
	t=readitem(srwp,n);

	return(t);
}
static char *cvt(str)
char *str;
{
int tmp;
static char bb[14];
	*bb=0;
	switch(*(str+1)) {
	case 'm':  
		sprintf(bb,"%d\n",mon_end((time_t)readitem(srwp,-srwp->skip)));
		return(bb);
	case 'j':  
		sprintf(bb,"%d\n",
			jday((time_t)readitem(srwp,-srwp->skip)));
		return(bb);
	default:break;
	}
	return(str);
}
