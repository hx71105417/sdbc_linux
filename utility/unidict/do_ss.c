/*   do_ss.c 2003.9.6  */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <errno.h>
#include <malloc.h>
#include <strproc.h>


/* in do_line.c */
extern int do_line(char *);



extern int errno;
int ssinit(void);


double *l=0;
int labn;
static char merr[]="\n%s ´æ´¢Ê§°Ü!\n";

static char ss_src[]="(-{0,1}[0-9]+)\\.\\.(-{0,1}[0-9]+)" ;
static char ss1_src[]= "(-{0,1}[0-9]+)&&" ;
//static char *ss,*ss1;
static regex_t ss,ss1;
#define MAXREG 5

int do_ss(label)
char *label;
{
char *p,*p1,*sp,s[10],e[10];
int cc,i,j,k,start,end;
int err;
regmatch_t pmatch[MAXREG];
	err=0;
	if(!label || !*label) return 1;
	if(*label==';') {
		return do_line(label);
	}
	if(!regexec(&ss,label,MAXREG,pmatch,0)) { /* ,,,1..80=  */
          char *__loc1;
		__loc1=label+pmatch[0].rm_so;
		sp=label+pmatch[0].rm_eo;
		i=pmatch[1].rm_eo-pmatch[1].rm_so;
		strncpy(s,label+pmatch[1].rm_so,i);
		s[i]=0;
		i=pmatch[2].rm_eo-pmatch[2].rm_so;
		strncpy(e,label+pmatch[2].rm_so,i);
		e[i]=0;
		p=malloc(end=(1+strlen(label)));
		p1=malloc(end);
		if(!p) {
			fprintf(stderr,merr,"do_ss,1..80");
			return(1);
		}
		start=atoi(s);
		end=atoi(e);
		strcpy(p,sp);
		sprintf(__loc1,"%%d%s",p);
		strcpy(p,label);
		for(i=start;i<=end;i++) {
			sprintf(label,p,i);
			while(!regexec(&ss1,label,MAXREG,pmatch,0)) { // 1&&
				__loc1=label+pmatch[0].rm_so;
				sp=label+pmatch[0].rm_eo;
				k=pmatch[1].rm_eo-pmatch[1].rm_so;
				strncpy(e,label+pmatch[1].rm_so,k);
				e[k]=0;
				k=atoi(e);
				j=k+i-start; 
				strcpy(p1,sp);
				sprintf(__loc1,"%%d%s",sp);
				strcpy(p1,label);
				sprintf(label,p1,j);
			}
			cc=do_line(label);
			if(cc) err=cc;
		}
		free(p);
		free(p1);
	} else {
		cc=do_line(label);
		if(cc) err=cc;
	}
	return err;
}

int ssinit()
{
	labn=10;
	return (
	((l=(double *)malloc(sizeof(double) * labn))!=0)&&
	(!regcomp(&ss,ss_src,REG_EXTENDED)) &&
	(!regcomp(&ss1,ss1_src,REG_EXTENDED)));
}

ssfree()
{
	myfree((char *)l);
	regfree(&ss);
	regfree(&ss1);
}
