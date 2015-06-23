/*   zdcl.c 91.9.  */
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include "calc.h"
#include <string.h>
#include "srw.h"
#define LABN 10

extern char merr[];
extern struct srw_s Table; 
struct srw_s *srwp=&Table;
extern char *Showid;

char *myname;

extern char *basename(char *);
void quit(int);

struct tm times,*localtime();

double getdata();
int putdata(int,int,char *[CALCID],char *);
time_t today;

int main(argc,argv)
int argc;
char *argv[];
{
FILE *fp,*fopen();
int cc;
long number;
double x;
	nice(10);
	tzset();
	myname=basename(argv[0]);
	Showid=myname;
	today=rtoday();
	signal(SIGTERM,quit);/*shut down*/
	signal(SIGPIPE,quit);
	signal(SIGINT,quit);
	signal(SIGHUP,quit);/*modem hung up*/
	signal(SIGPWR,quit);/*UPS is weak*/

	if(argc>1) today=cvtdate(argv[1],today);
	cc=ssinit();
	if(!cc) {
fprintf(stderr,"after ssinit() ret=%d\n",cc);
		fprintf(stderr,merr,myname);
		quit(1);
	}
	
	initsrw(&Table);
	fp=stdin;
	cc=do_dic(fp,1);
	quit(cc);
}
void quit(int n)
{
	ssfree();
	___SQL_CloseDatabase__(&Table.SQL_Connect);
	freesrw(&Table);
	exit(n);
}

#include "putdata.c"
