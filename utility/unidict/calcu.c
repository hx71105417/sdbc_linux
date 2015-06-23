/* @(#) calc.c 1.4 91/04/19 */
#include <stdio.h>
#include <ctype.h>
#include <regex.h>
#include <sdbc.h>

#include "calc.h"

struct args {
	double (*getdata)(int,int,char *[CALCID]);
	int argc;
	char *argv[CALCID];
	char argbuf[130];
	char *cp;
};

/*double getdata(int,char,int,char[CALCID][CALCIDLEN]);*/
static char * setarg(struct args *);
static double cal(double,int,struct args *),F0=0.;

static char Moper_src[]="^[ 	]*(-{0,1}[0-9]{1,5})[+][+](-{0,1}[0-9]{1,5})";
static regex_t Moper;
static int regflg=0;
#define MAXREG 5
static regmatch_t pmatch[MAXREG];
/* 
static char Moper[] = {
040,031,03,040,011,027,055,00,01,033,04,020,
060,071,01,05,030,02,053,030,02,053,074,00,
027,055,00,01,033,04,020,060,071,01,05,014,
00,00,064,
0};
*/

static char *__loc1;
double calculator(str,getdata)
char *str;
double (*getdata)(int,int,char *[CALCID]);
{
struct args arg;
	if(!str||*str=='#'||*str==';') return 0.;
	if(!regflg) {
	int ret;
	char errbuf[200];
		ret=regcomp(&Moper,Moper_src,REG_EXTENDED);
		if(ret) {
			regerror(ret,&Moper,errbuf,sizeof(errbuf));
			fprintf(stderr,"%s\n",errbuf);
			return 0;
		}
		regflg=1;
	}
	arg.getdata=getdata;
	arg.cp=str;
	return(cal(0.,ADD,&arg));
}

static double cal(x,op,arg)
double x;
int op;
register struct args *arg;
{
char *rp;
double tmp;
union {
	int n;
	long l;
	} m;

	if(!*arg->cp) return(calc(x,op,0.));
	tmp=F0,m.l=0;
	do {

		if(*arg->cp=='('/*)*/) {
			arg->cp++;
			tmp=cal(0.,ADD,arg);
			rp=arg->cp;
			goto oper;
		}
		setarg(arg);
		if(*arg->cp<=' ') {
			arg->cp++;
			if(!*arg->cp) x=calc(x,op,tmp);
			continue;
		}
		m.l=0;
		if(tolower(*arg->cp)=='c') {
			tmp=strtod(arg->cp+1,&rp);	
			if(isdigit(*rp)) rp++;
			if(rp==arg->cp+1) {
				arg->cp=++rp;
				if(!*rp)  {
					x=calc(x,op,tmp);
					break;
				}
				continue;
			}
		}
		else {
		char right[7];
		char *p;
			tmp=strtod(arg->cp,&rp);	
			if(rp!=arg->cp) {
				if(isdigit(*rp)) rp++;
				if(!arg->getdata) goto oper;
				for(p=arg->cp;p<rp;p++) {
					if(*p=='.' || *p=='E' || *p=='e') {
						goto oper;
					}
				}
			}
			if(!regexec(&Moper,arg->cp,MAXREG,pmatch,0)) {
			int i;

				p=arg->cp+pmatch[0].rm_eo;
				__loc1=arg->cp+pmatch[1].rm_so;
				i=pmatch[2].rm_eo-pmatch[2].rm_so;
				strncpy(right,arg->cp+pmatch[2].rm_so,i);
				right[i]=0;
		
/* XXXXX   1++80  
fprintf(stderr,"calc:left=%s, right=%s\n",lft,right);
 */
			   tmp=F0;
			   if(isalpha(*p)) *arg->argv[0]=*p++;
			   else *arg->argv[0]=0;
			    m.l=atoi(right);
			    for(i=atoi(__loc1);i<=m.n;i++) {
			       if(arg->getdata)
			          tmp+=(*arg->getdata)(i,arg->argc,arg->argv);
			       else tmp+=(double)i;
			    }
			    rp=p;
			}
			else {
			   if(*arg->cp) m.l=strtol(arg->cp,&rp,0);
			   if(isalpha(*rp)) *arg->argv[0]=*rp++;
			   else *arg->argv[0]=0;
			   if(arg->getdata) {
			      tmp=(*arg->getdata)(m.n,arg->argc,arg->argv);
			   }
			   else tmp=(double)m.l;
			}
		}
oper:
		if(!*rp) {
			x=calc(x,op,tmp);
			arg->cp=rp;
			break;
		}
		while(isspace(*rp)) rp++;
		switch(*rp) {
		case '+': m.n=ADD;
			  rp++;
			  break;
		case '-': m.n=SUBT;
			  rp++;
			  break;
		case '*': m.n=MUL;
			  rp++;
			  break;
		case '/': m.n=DIV;
			  rp++;
			  break;
		case '%': m.n=MOD;
			  rp++;
			  break;
/*
		case ')':
printf("op=%d,x=%f m.n=%d tmp=%f rp=%s\n",op,x,m.n,tmp,rp);
			  if(*rp) rp++;
			  if(x==0. && op==0) {
				return tmp;
			  }
			  m.n=-1;
			  break;
*/
		case '\n':
		case '\r':
		case ';':
		case '#': *rp=0;
		default: m.n=-1;
			  if(*rp) rp++;
			  break;
		}
		arg->cp=rp;

		if(m.n==-1) {
			x=calc(x,op,tmp);
			break;
		}
		else if((m.n>>2) > (op>>2)){
			tmp=cal(tmp,m.n,arg);
			rp=arg->cp;

			goto oper;
		}
		x=calc(x,op,tmp);
		if((m.n>>2) == (op>>2)) {
			op=m.n;
		}
		else  {
			arg->cp--;
			break;
		}

	} while(*arg->cp);
	return(x);
}

let(str,x,putdata)
char *str;
double x;
int (*putdata)(int,int,char *[CALCID],double);
{
struct args arg;
int n=0;
char *p;
	arg.cp=str;
	setarg(&arg);
	n=strtol(arg.cp,&p,0);
	if(p==arg.cp) return(-1);
	if(p&&isalpha(*p)) *arg.argbuf=*p;
	return((*putdata)(n,arg.argc,arg.argv,x));
}
static char * setarg(arg)
register struct args *arg;
{
int n;
register char *rp;
char *p;
	arg->argc=0;
	*(arg->argv[0]=arg->argbuf)=0;
	for(n=1;n<CALCID;n++) {
		*(p=arg->argv[n]=arg->argv[n-1]+strlen(arg->argv[n-1])+1)=0;
again:
		if((p-arg->argbuf) >= sizeof(arg->argbuf)-1) 
			break;
		rp=stptok(arg->cp,p,
			sizeof(arg->argbuf)-(p-arg->argbuf),
			",+*-/%()#;");
		if(*rp && rp[-1]=='\\') {
			p=arg->argv[n]+strlen(arg->argv[n])-1;/* del \\ */
			*p++=*rp++;
			*p=0;
			arg->cp=rp;
			goto again;
		}
		if(*rp!=',') break;
		arg->cp=rp+1,arg->argc=n;
	}
	return(arg->cp);
}
int strlet(label,str,putdata)
char *label;
char *str;
int (*putdata)(int,int,char *[CALCID],char *str);
{
struct args arg;
int n=0;
char *p;
	arg.cp=label;
	setarg(&arg);
	n=strtol(arg.cp,&p,0);
	if(p==arg.cp) return(-1);
	if(p&&isalpha(*p)) *arg.argbuf=*p;
	return((*putdata)(n,arg.argc,arg.argv,str));
}
