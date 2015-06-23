/*   s16.c 91.9.  */
#pragma pack()
#include <stdio.h>
#include <signal.h>
#include "srw.h"
#include "calc.h"
#include <string.h>
#define LABN 10
#define DBWR


time_t today;
char *myname;

/* define in zdline.c */
extern char merr[];
extern struct srw_s Table; 
struct srw_s *srwp=&Table;

void quit (int);
static void intr();
double getdata();
int putdata(int,int,char *[CALCID],char *);
char *basename(char *);
main(argc,argv)
int argc;
char *argv[];
{
FILE *fp,*fopen(),*popen();
int cc;
char line[512];
long number;
double x;
	nice(10);
	tzset();
	signal(SIGTERM,intr);/*shut down*/
	signal(SIGPIPE,intr);
	signal(SIGINT,intr);
	signal(SIGHUP,intr);/*modem hung up*/
	signal(SIGPWR,intr);/*UPS is weak*/
	myname=basename(argv[0]);
	Showid=myname;
	cc=ssinit();
	if(!cc) {
		fprintf(stderr,merr,myname);
		quit(1);
	}
	today=rtoday();

	if(argc < 2) {
		fprintf(stderr,"Dic name?\n");
		quit(1);
	}
	if(argc==3) {
		today=cvtdate(argv[2],today);
	}
	if(*argv[1]=='@') {
		sprintf(line,"zdsc %s",argv[1]+1);
		fp=popen(line,"r");
		if(!fp) {
			perror(line);
			exit(1);
		}
	} else {
		fp=fopen(argv[1],"r");
		if(!fp) {
			perror(argv[1]);
			exit(1);
		}
	}
/*
	for(cc=0;cc<argc;cc++)
		fprintf(stderr,"%s\n",argv[cc]);
*/
	do_dic(fp,1);
	if(*argv[1]=='@') {
		pclose(fp);
	} else fclose(fp);
	quit(0);
}
static void intr()
{
	quit(255);
}
void quit(int n)
{
	ssfree();
	if(srwp->W) writedb(srwp);
	___SQL_Transaction__(&(srwp->SQL_Connect),TRANCOMMIT);
	___SQL_CloseDatabase__(&Table.SQL_Connect);
	freesrw(&Table);
	exit(n);
}

#include "putdata.c"
