/********************************************************************
 *   chkrk.c 2003.07.28 by ylh  
 * usage:chkrk dict [date] <data >check_message
 * the data file as: 2007.06.02|BJ|XB1|101|9|2|3|4|5|6|7|8|0|0|
 * usage:chkrk @dict.zdsc [date] <data >check_message
 * 这个程序写得比较死，统计文件必须是：日期，单位，表名，标志。
 * 当日合格数据的标志是101，见chkdada()函数.
 * 项号无定语，指当前表，一个定语是表名，两个是日期，表名。
 ********************************************************************/
 
#pragma pack()
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "srw.h"
#include "calc.h"
#include <string.h>
#define LABN 10
#define DBWR

/* define in srw.c */
extern struct srw_s Table; // in srw.c
extern char *Showid;
extern FILE *logfile;
time_t today;
char *myname;

struct srw_s *srwp=&Table;

void quit (int);
static void intr();
double getdata();
int putdata(int,int,char *[CALCID],char *);
main(argc,argv)
int argc;
char *argv[];
{
FILE *fp,*fopen(),*popen();
int cc;
char line[512],*cp;
char zdfile[512];
long number;
//double x;
char *p,buf[1024],ofilename[512];
	nice(10);
	tzset();
	signal(SIGTERM,intr);/*shut down*/
	signal(SIGPIPE,intr);
	signal(SIGINT,intr);
	signal(SIGHUP,intr);/*modem hung up*/
	signal(SIGPWR,intr);/*UPS is weak*/
	myname=argv[0];
	today=rtoday();
	if(argc < 2) {
		ShowLog(2,"%s Dic name?\n",argv[0]);
		quit(1);
	}
	Showid=argv[0];
	cp=getenv("DIR18D");

	if(argc==3) {
		today=cvtdate(argv[2],today);
	}
	if(*argv[1]=='@') {
		if(cp&&*cp&&argv[1][1]!='/') {
			sprintf(zdfile,"%s/%s",cp,argv[1]+1);
		} else strcpy(zdfile,argv[1]+1);
		sprintf(line,"zdsc %s",zdfile);
		fp=popen(line,"r");
		if(!fp) {
			perror(line);
			exit(1);
		}
	} else {
		if(cp&&*cp&&*argv[1]!='/') {
			sprintf(zdfile,"%s/%s",cp,argv[1]);
		} else strcpy(zdfile,argv[1]);
		fp=fopen(zdfile,"r");
		if(!fp) {
			perror(zdfile);
			exit(1);
		}
	}
	ssinit();
	if(argc<4){
		cc=do_chk(fp,1);
		chk_update_db(cc);
	} else { while(!ferror(fp)) {
		fgets(buf,sizeof(buf),fp);
		if(feof(fp)) break;
		p=skipblk(buf);
		if(!*p || *p=='#') continue;
		if(sscanf(p,"TABDESC=%s",ofilename)==1) {
			if(!Table.tabname) {
				cc=MkTabDesc(ofilename,&Table);
				if(cc) {
					exit(1);
				}
				cc=opendb(&Table);
				if(cc) {
					ShowLog(1,
						"Open database err %d",cc);
					return cc;
				}
				for(cc=0;cc<Table.skip;cc++)
					put_one(Table.rec,"",Table.srw_type,cc,'|');
				loaddata(&Table); // load a data from stdin
//	ptrec(srwp);
			}
			continue;
		}
	} chk_update_db(0); }
	printf("******\n");
	if(*argv[1]=='@') {
		pclose(fp);
	} else fclose(fp);
	ssfree();
	quit(0);
}
static void intr()
{
	quit(255);
}
void quit(int n)
{
	if(srwp->W) writedb(srwp);
	___SQL_Transaction__(&(srwp->SQL_Connect),TRANCOMMIT);
	___SQL_CloseDatabase__(&Table.SQL_Connect);
	freesrw(&Table);
	exit(n);
}

/* in cal.c */
extern int nullmod;

extern int errno;
//char *malloc(),*realloc();

char  *skipblk();


double chkdata(int,int,char *[CALCID]);

int do_chk(fd)
FILE *fd;
{
register char *p;
int cc,err=0;
int ff=1;
double x,y;
char ofilename[256],*p1,*cp;
char fmd[10],buf[1024];
	while(!ferror(fd)) {
		fgets(buf,sizeof(buf),fd);
		if(feof(fd)) break;
		p=skipblk(buf);
		if(!*p || *p=='#') continue;
		if(sscanf(p,"TABDESC=%s",ofilename)==1) {
			if(!Table.tabname) {
				cc=MkTabDesc(ofilename,&Table);
				if(cc) {
					exit(1);
				}
				cc=opendb(&Table);
				if(cc) {
					ShowLog(1,
						"Open database err %d",cc);
					return cc;
				}
				for(cc=0;cc<Table.skip;cc++)
					put_one(Table.rec,"",Table.srw_type,cc,'|');
				loaddata(&Table); // load a data from stdin
//	ptrec(srwp);
			}
			continue;
		}
/* How meny records loaded will be commit */
		if(sscanf(p,"COMMITNUM=%d",&Table.commit)==1) continue;
		if(sscanf(p,"NULLMOD=%d",&nullmod)==1) continue;
		p1=p;
		while(cp=strchr(p1,'#')) {
			if(cp[-1]=='\\') {
				strdel(cp-1);
				p1=cp+1;
				continue;
			}
			*cp=0;
			break;
		}
/*
//fprintf(stderr,"today=%d\n",today);
		if(!chkstr(p,&x,&y,chkdata)) {
			err=1;
* 凡校验不成功的行(err=1)将校验字典的该行及计算后的左右值在校验行提示 *
			printf("%lg(%lg).........%s",x,y,p); 
		}
*/
		cc=do_ss(p);
		if(cc) err=cc;
	}
	return err;
}

double chkdata(n,argc,argv)
int n,argc;
char *argv[CALCID];
{
char tname[20],date[30],date1[30];
int i;
/*
for (i=0;i<argc;i++) {
	ShowLog(2 ,"chkdata %d:%s,",i,argv[i+1]);
}
*/
	get_one(date1,srwp->rec,srwp->srw_type,0,'|');
	rjulstr(date,today);
	get_one(tname,srwp->rec,srwp->srw_type,2,'|');
	if(argc==1) {
		if(strcmp(srwp->rowid,tname)) srwp->W=0;
		s(srwp,3,argv[1]);  // tabname
	}
	else if(argc==2) {
		rjulstr(date1,cvtdate(argv[1],today));
		if(strcmp(srwp->rowid,tname)||strcmp(date,date1)) srwp->W=0;
		s(srwp,1,argv[1]);  // date
		s(srwp,3,argv[2]);  // tabname
	}

	get_one(tname,srwp->rec,srwp->srw_type,2,'|');
//ShowLog(2,"tname1=%s,name2=%s,date=%s,date1=%s",
//		tname,srwp->rowid,date,date1);
	if(!strcmp(tname,srwp->rowid)&&!strcmp(date,date1))
			 put_one(srwp->rec,"-1",srwp->srw_type,3,'|');
	else	put_one(srwp->rec,"101",srwp->srw_type,3,'|');
	return(readitem(srwp,n));
}

loaddata(struct srw_s *srwp)
{
char buf[2048],stmt[1024];
int ret;
char date[30];
	clrec(srwp);
	fgets(buf,sizeof(buf),stdin);
	if(feof(stdin) || ferror(stdin)) return -1;
	TRIM(buf);
	chk_dispack(srwp->rec,buf,srwp->srw_type);
	srwp->W=-1;
	srwp->R=0;
//	*(time_t *)srwp->rec=cvtdate(buf,today);
/* 未进行校验前，先按无效数据入库 */
	put_one(srwp->rec,"-1",srwp->srw_type,3,'|');

/* 当前日期改为数据日期 */
	get_one(date,srwp->rec,srwp->srw_type,0,'|');
	today=rstrjul(date);

	get_one(srwp->rowid,srwp->rec,srwp->srw_type,2,'|'); // save tabname into rowid
	sprintf(stmt,"delete %s where %s",srwp->tabname,
			mkwhere(srwp,buf));
	___SQL_Transaction__(&(srwp->SQL_Connect),TRANBEGIN);
	ret=___SQL_Exec(&srwp->SQL_Connect,stmt);
	ShowLog(5,"loaddata:%d, %s\n",ret,stmt);
	strcpy(buf,"loaddata:");
	ret=insert_db(&srwp->SQL_Connect,srwp->tabname,srwp->rec,
						srwp->srw_type,buf);
	if(ret) {
		ShowLog(1,"err=%d,stmt=%s ",srwp->SQL_Connect.Errno,buf);
		___SQL_Transaction__(&(srwp->SQL_Connect),TRANROLLBACK);
	}
	return 0;
}

/* 与net_dispack不同，数据项数可以比模板项数少 */
int chk_dispack(void *net_struct,char *buf,T_PkgType *pkg_type)
{
int i,k;
char *cp;
char *cp1,*cp2;
int ali;
char tmp[80];
	if(buf) cp=buf;
	else return -1;
	cp2=cp;
	if(!cp)return LENGERR;
	if(pkg_type->offset) set_offset(pkg_type);
	for(i=0;pkg_type[i].type>-1;i++){
		cp1=strchr(cp,'|');
		if(cp1) *cp1++=0;
		else return FORMATERR;
		put_one(net_struct,cp,pkg_type,i,'|');
		cp=cp1;
		if(!*cp1) break;
	}
	return (cp-cp2);
}

chk_update_db(int flg)
{
char stmt[2048],buf[1024],value[1536],tmp[2048];
int ret;
char *wp,*dp;
char curname[9];
char date[30],date1[30];
double x;
	rjulstr(date,today);
//ShowLog(2,"update_db:flg=%d,date=%s\n",flg,date);
	get_one(curname,srwp->rec,srwp->srw_type,2,'|');
	get_one(date1,srwp->rec,srwp->srw_type,0,'|');
	if(strcmp(srwp->rowid,curname)||strcmp(date,date1)) {
		s(srwp,1,date);
		s(srwp,3,srwp->rowid);
		s(srwp,4,"-1");
		x=readitem(srwp,0);
		if(x) {
			ShowLog(1,"ReRead tab %s err %d",srwp->rowid,(int)x);
			return;
		}
	}
	if(flg) {
		wp="-1";dp="101";
	} else {
		wp="101";dp="-1";
	}
	put_one(srwp->rec,dp,srwp->srw_type,3,'|');
	sprintf(stmt,"delete %s where %s",srwp->tabname,
			mkwhere(srwp,buf));
	ret=___SQL_Exec(&srwp->SQL_Connect,stmt);
	ShowLog(5,"%s:ret=%d\n",stmt,ret);
	put_one(srwp->rec,wp,srwp->srw_type,3,'|');
	srwp->R=-1;
	strcpy(tmp,"chk_update_db:");
	ret=insert_db(&srwp->SQL_Connect,srwp->tabname,srwp->rec,
							srwp->srw_type,tmp);
	if(ret && srwp->SQL_Connect.Errno==DUPKEY) {
		net_pack(buf,srwp->rec,srwp->srw_type);
		sprintf(stmt,"update %s %s where %s",
			srwp->tabname,
			mkupdate(value,buf,srwp->srw_type),
			mkwhere(srwp,tmp));
		ret=___SQL_Exec(&srwp->SQL_Connect,stmt);
		ShowLog(5,"%s:ret=%d\n",stmt,ret);
	} else ShowLog(5,"%s\n",tmp);
	srwp->W=0;
}
int do_line(char *p)
{
double x,y;
//fprintf(stderr,"today=%d\n",today);
	if(*p==';') {
		printf("%s",p+1);
	} else if(!chkstr(p,&x,&y,chkdata)) {
/* 凡校验不成功的行(err=1)将校验字典的该行及计算后的左右值在校验行提示 */
		printf("%.0lf(%.0lf).........%s",x,y,p); 
		return 1;
	}
	return 0;
}
