/* chk_mid.c 校验*/
#include "srw.h"
#include "calc.h"

extern char *stptok();
void quit(int n);
/* in showlog.c */
extern char *Showid;
/* in srw.c */
char *myname;

/* in zdline.c */
extern FILE *ofd;
extern struct srw_s Table;
struct srw_s *srwp=&Table;
extern char *merr;

time_t today;
double getdata(int,int,char *[CALCID]);
main(argc,argv)
int argc;
char *argv[];
{
char *p,buf[2048],ofilename[400];
int n,cc;

ofd=stdout;
	Showid=argv[0];
/*
	"usage : %s  date unit  [flg] <chkdict \n",argv[0]);
		  0   1    2      3      
*/
	if(argc < 2) {
		fprintf(stderr,
	"usage : %s  date unit  [flg] <chkdict \n",argv[0]);
		quit(1);
	}
	today=rtoday();
	if(!ssinit()) { /* zdline.c */
		fprintf(stderr,merr,myname);
		quit(1);
	}
	while(!ferror(stdin)) {
		fgets(buf,sizeof(buf),stdin);
		if(feof(stdin)) break;
		p=skipblk(buf);
		if(!*p || *p=='#') continue;
		if(sscanf(p,"TABDESC=%s",ofilename)==1) {
			cc=MkTabDesc(ofilename,&Table);
			if(cc) {
				ssfree();
				exit(1);
			}
			cc=opendb(&Table);
			if(cc) {
				fprintf(stderr,
					"Open database err %d",cc);
				return cc;
			}
			for(cc=0;cc<Table.skip;cc++)
				put_one(Table.rec,"",Table.srw_type,cc,'|');
			break;
		}
	}

	put_one(srwp->rec,"101",srwp->srw_type,3,'|'); //flg
/* 设置日期 站名 分子 [标识] */
	switch(argc) {
	case 4:
		s(srwp,4,argv[3]);              /* flg */
	case 3:
		s(srwp,2,argv[2]);                /* unit */
	case 2:
		today=cvtdate(argv[1],today);
		s(srwp,1," ");                    /* s18date */
		break;
	default: 
		fprintf(stderr,
	"usage : %s  date unit  [flg] <chkdict \n",argv[0]);
		quit(1);
		break;
	}
	do_dic(stdin,1); /* zdline.c */
	ssfree(); /* zdline.c */
	puts("******");
	return 0;
}

do_line(str)
char *str;
{
double x,y;
int cc;
	if(*str==';') {
		fprintf(ofd,"%s\n",str+1);
	}
	if(!(cc=chkstr(str,&x,&y,getdata))) {
		fprintf(ofd,"%lg(%lg)...........%s\n",x,y,str);
	}
//fprintf(ofd,"DBG %lg(%lg)...%s,cc=%d\n",x,y,str,cc);
	return;
}
static void intr(int n)
{
	quit(n);
}
void quit(int n)
{
	if(srwp->W) writedb(srwp);
	___SQL_Transaction__(&(srwp->SQL_Connect),TRANCOMMIT);
	___SQL_CloseDatabase__(&Table.SQL_Connect);
	freesrw(&Table);
	exit(n);
}
extern double *l;
extern int labn;
extern FILE *ofd;

double getdata(n,argc,argv)
int n,argc;
char *argv[CALCID];
{
	int i,cc;
	double t;
/*
fprintf(stderr,"getdata:");
for(i=1;i<=argc;i++) fprintf(stderr,"%s,",argv[i]);
fprintf(stderr,"%d%c  \n",n,*argv[0]);
*/
	srwp->rp=NULL;
	if (argc==0&&*argv[0]) {  // 1L   6D ...
		switch(*argv[0]) {
		case 'L':  /* 处理标号表达式*/
			if(l && n>=0 && n<labn) {
//fprintf(stderr,"labn=%d,L[%d]=%f\n",labn,n,l[n]);
				return(l[n]);
			}
			else  return(0.);
		case 'D': /* func in cvtdate.c */
			if(n==0) t=(double)mday(today);		
	
			else if(n==1) {    /* month end day */
				t=(double)mday((time_t)readitem(srwp,-srwp->skip));
			}
			else if(n==2) {    /* day of mon */
				t=(double)mon_end((time_t)readitem(srwp,-srwp->skip));
			}
			else if(n==3) {   /* day of year */
				t=(double)yday((time_t)readitem(srwp,-srwp->skip));
			}
			else if(n==4) { /* mon of today */
				t=(double)mon_end(today);
			}
			else if(n==5) { /* mon of rec */
				t=(double)mon_end((time_t)readitem(srwp,-srwp->skip));
			}
			else if(n==6) t=(double)today;
			else if(n==7) t=(double)jday((time_t)readitem(srwp,-srwp->skip));
/* year of rec */
			return(t);
		default: return(0.);
		}
	} else if (*argv[0] == 'D' && argc == 1) { //  .\-1,6D
		switch(n) {
		case 0: t=(double)mday(cvtdate(argv[1],today));		
			break;
		case 1: 
			t=(double)mday(cvtdate(argv[1],
					(time_t)readitem(srwp,-srwp->skip)));		
			break;
		case 2:     /* day of mon */
			t=(double)mon_end(cvtdate(argv[1],
					(time_t)readitem(srwp,-srwp->skip)));
			break;
		case 3:    /* day of year */
			t=(double)yday(cvtdate(argv[1],
					(time_t)readitem(srwp,-srwp->skip)));
			break;
		case 4:  /* mon of today */
			t=(double)mon_end(cvtdate(argv[1],today));
			break;
		case 5:  
			t=(double)mon_end(cvtdate(argv[1],
					(time_t)readitem(srwp,-srwp->skip)));
			break;
		case 6: t=(double)cvtdate(argv[1],today);
			break;
		case 7:    /* day of season */
			t=(double)jday(cvtdate(argv[1],
					(time_t)readitem(srwp,-srwp->skip)));
			break;
		default: return(0.);
		}
		return(t);
	} else { /*填充索引场值*/
		srwp->W=0;
		if(argc==1) {
			s(srwp,3,argv[1]);  // tabname
		}
		else if(argc==2) {
			s(srwp,1,argv[1]);  // date
			s(srwp,3,argv[2]);  // tabname
		}
		return(readitem(srwp,n));
	}
}
