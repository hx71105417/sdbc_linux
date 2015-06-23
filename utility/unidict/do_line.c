/*   zdcl.c 91.9.  */
#include <stdio.h>
#include <signal.h>
#include "calc.h"
#include <string.h>
#include <ctype.h>
#include "srw.h"
extern struct srw_s *srwp;
char *strlower(char *);
char *strupper(char *);


/* in getdata.c */
extern double getdata(int,int,char *[CALCID]);

/* in putdata.c */
int putdata(int,int,char *[CALCID],char *);

/* in compmain.c srw.c */
extern char *myname;
double calprt(char *); 
extern char merr[];

extern int errno;
extern FILE *ofd;

void do_line(char *);
void do_line(line)
char *line;
{
unsigned char label[140],*sp,s[10],e[10],*p1;
register unsigned char *p;
char aa[82];
int cc,i,start,end;
double x,y;

	if(*line==';') {
		calprt(line);
		return ;
	}
	if(*(p=(unsigned char *)stptok(line,(char *)label,sizeof(label),"=")))
		if((*p != '=')|| (p[1]=='=')) { 
/* line too long, >sizeof(label) */
			p=(unsigned char *)line;*label=0;
		} else {
		/*将标号分离出来*/	
			p++;
			p=(unsigned char *)skipblk((char *)p);
			if(!*p || *p==';' || *p=='#') { /* load from stdin */
				x=0;
				fgets(aa,sizeof(aa),stdin);
				if(feof(stdin)) quit(254);
				TRIM(aa);
	
				cc=strlet((char *)label,aa,putdata);	
				if (cc){
					fprintf(stderr,merr,"do_line");
					quit(255);
				}
				if(*p==';') {
					prt(x,p+1);
				}
				return ;
			}
		}
	else *label=0,p=(unsigned char *)line;
	x=calprt((char *)p);/*将没有标号的行送calprt处理*/
	if(*label) {
			/* 以标号做为下标将返回值存储 */
		sprintf(aa,"%lf",round(x));
		cc=strlet((char *)label,aa,putdata);	
		if (cc){
fprintf(stderr,"after strlet() cc=%d\n",cc);
			fprintf(stderr,merr,myname);
			quit(255);
		}
	}
}
double calprt(p)/*计算表达式,按格式输出*/
char *p;
{
double x;
register char *s;
int n;
	if(*p==';') { 
/*
		if(s=strchr(p,'#')) {
			if(s[-1] == '\\') strdel(s-1);
			else *s='\0';
			TRIMlf(p);
		}
*/
		n=strlen(p);
		if(p[n-1]=='&') {/* 续行 */
			p[n-1]='\0';
			n=0;
		}
		fprintf(ofd,"%s",p+1);
		if(n) fputc('\n',ofd);
		return(0.);
	}
	if ((s=strrchr(p,';'))) s++;
	x=calculator(p,getdata);
	prt(x,s);
	return(x);
}

prt(x,fmt)
double x;
register char *fmt;
{
int n,cc,i;
int m=0,prnl=1;
short ymd[3];
int caseflg;
double y;
char prtfmt[20],*p;

	caseflg=0;
	if(!fmt) return; /*此行值不输出*/
	n=strlen(fmt)-1;
	if(fmt[n]=='&') { /* 续行 */
 		prnl=0;
		fmt[n]=0;
	}
	p=0;
	if(*fmt) {
		for(p=fmt+strlen(fmt);!isdigit(p[-1])&&p>fmt;p--) ;
	}
	if(isnull(&x,CH_DOUBLE)) {
		if(p) fprintf(ofd,"%s",p);
		if(prnl) fputc('\n',ofd);
		return;
	}
		
	rjulymd((time_t)x,ymd);
	strcpy(prtfmt,"%.*lf");
	n=0;
	switch(*fmt) {
	case 'g':
		fprintf(ofd,"%lg",x);
		fmt++;
		break;
	case 'Y':

		fprintf(ofd,"%d-%02d-%02d",
			ymd[0],ymd[1],ymd[2]);
		fmt++;
		break; 
	case 'y':
		fprintf(ofd,"%d", ymd[0]);
		fmt++;
		break; 
	case 'm':
		fprintf(ofd,"%02d", ymd[1]);
		fmt++;
		break; 
	case 'd':
		fprintf(ofd,"%02d", ymd[2]);
		fmt++;
		break; 
	case 'j':
		fprintf(ofd,"%d",(ymd[1]-1)/3+1);
		fmt++;
		break; 
/*  ;+5    145 ->  +145  */
	case '+':
		sprintf(prtfmt,"%%%c.*lf",*fmt);
		n=1; 
		fmt++;
		break;
/* 2005-09-28 by ylh  ************
	case ' ':
		fmt++;
*****************************************/
/*  ;04    145 ->  0145  */
        case '0':
		n=strtol(fmt,&p,10);
		if(!n) {
			n=-2; 
			break;
		}
		sprintf(prtfmt,"%%%c%dd",*fmt,n);
		n=(int)f_round(x,5,0);
		fprintf(ofd,prtfmt,n);
		fmt=p;
		n=0;
		break; 
	case 'u': 
		fmt++;
		caseflg=1;
		n=-1;
		break;
	case 'l': 
		fmt++;
		caseflg=2;
		n=-1;
		break;
        default:
		n=-1;
		break;
	}
	if(!n) {
		fprintf(ofd,"%s",fmt);
		if(prnl) fputc('\n',ofd);
			return;
	}
	if(srwp->rp) {
		n=sscanf(fmt,"%lf",&y);
		if(n==1) {
		char ff[20];
			sprintf(ff,"%g",y);
			sprintf(prtfmt,"%%%ss",ff);
		} else strcpy(prtfmt,"%s");
		switch(caseflg) {
		case 1:
			strupper(srwp->rp);
			break;
		case 2:
			strlower(srwp->rp);
			break;
		default:
			break;
		}
		fprintf(ofd,prtfmt,srwp->rp);

		if(*fmt) fprintf(ofd,"%s",fmt);
		if(prnl) fputc('\n',ofd);
		return;
	}
	if(n!=-2) n=-1;
	else n=0;
	m=0;
	m=strtol(fmt,&p,10);
	if(*p=='.') {
		p++; 
		fmt=p;
		n=strtol(fmt,&p,10);
		if(p==fmt) {
			n=-1;
		} 
	}
	fmt=p;
	y=x;
	i=1;
	switch(n) {
 /* 不加权舍入 */
	case 1: /* 向-舍入 */
	case 2: /* 向+舍入 */
 	case 3: /*全舍*/
	case 4: /*全入*/
	case 5: /* 4舍5入 */
		x=f_round(x,n,m);
		break;
 /* 加权舍入 */
	case -1:
	    x=f__round(x,5,0);
	    if(m>=0) {
		for(cc=0;cc<m;cc++) i *= 10;
	    	x /= i;

	    }
	    else {
		for(cc=0;cc>m;cc--) i *= 10;
		x *= i;
	    }
	    break;
	case 0:  /* 保持0 */
	default:
		x=f_round(x,5,m);
		break;
	    break;
	}
	if(m<0) m=0;

	if(x || !n) fprintf(ofd,prtfmt,m,x);
	
	fprintf(ofd,"%s",fmt);
	if(prnl) fputc('\n',ofd);
	fflush(ofd);
}

