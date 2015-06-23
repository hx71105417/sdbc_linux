/*   zdline.c 2003.5.15  */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <regex.h>
#include "srw.h"
#include "calc.h"
static int iscondition(char *);

/* in srw.c */
extern struct srw_s Table; 

/* in getdata.c */
extern double getdata(int,int,char *[CALCID]);
/* in do_line.c */
extern void do_line(char *);


/* in getdb.c pdb.c */
extern char *myname;
extern quit();
/* in cal.c */
extern int nullmod;


extern int errno;
//char *malloc(),*realloc();
int ssinit(void);

char *skipblk();
static close_old_fd(FILE *);

static char buf[BUFSIZ];
FILE *ofd;
double *l=0;
int labn;
char merr[]="\n%s 存储失败!\n";

/********************************************************************
 * 2007.07.20 by yuliua
 * 原先是先做 logic，后做块语句
 * 改为先做块语句，后做logic 
 ********************************************************************/
static char ss_src[]="(-{0,1}[0-9]+)\\.\\.(-{0,1}[0-9]+)" ;
static regex_t reg_ss;
//static char ss1_src[]= "(-{0,1}[0-9]+)&&" ;
static char ss1_src[]= "([0-9]+)&&" ;
static regex_t reg_ss1;


unsigned char *TRIMlf(unsigned char *);
/* flg:逻辑标志，1=ture,else=false    */
do_dic(fd,flg)
FILE *fd;
int flg;
{
register char *p;
int cc;
int ff=1;/* 逻辑为假时的嵌套计数器 */
double x,y;
char ofilename[256],*p1,*cp;
char fmd[10];
	ofd=stdout;
	while(!ferror(fd)) {
		fgets(buf,sizeof(buf),fd);
		if(feof(fd)) break;
		p=skipblk(buf);
		if(!*p || *p=='#') continue;
		if(sscanf(p,"TABDESC=%s",ofilename)==1) {
			if(!Table.tabname) {
				substitute_env(ofilename);
				cc=MkTabDesc(ofilename,&Table);
				if(cc) {
					ssfree();
					exit(1);
				}
				cc=opendb(&Table);
				if(cc) {
					ShowLog(1,"Open database err %d",cc);
					ssfree();
					exit(3);
				}
				for(cc=0;cc<Table.skip;cc++)
					put_one(Table.rec,"",Table.srw_type,cc,'|');
			}
			continue;
		}
/* How meny records loaded will be commit */
		if(sscanf(p,"COMMITNUM=%d",&Table.commit)==1) continue;
		if(sscanf(p,"NULLMOD=%d",&nullmod)==1) continue;
		cc=sscanf(p,"LABN=%d",&labn);
		if(cc == 1) {
			l=(double *)realloc(l,labn*sizeof(double));
			continue;
		}
		cc=sscanf(p,"OUTFILE=%s %4s",ofilename,fmd);
		if(cc>=1) {
		   if(flg==1) {
			close_old_fd(ofd);
			ofd=fopen(ofilename,cc==2?fmd:"w");
			if(!ofd) {
				perror(ofilename);
				exit(1);
			}
		   }
		   continue;
		}
		else if(!strncmp(p,"OUTFILE=",8)) {
			close_old_fd(ofd);
			ofd=stdout;
			continue;
		}
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
		if(*p=='$') {
/* 遇到条件语句结束符，减嵌套计数，与之匹配的结束符返回true */
			if(--ff <= 0) return 1;
		}
		if(*p==':') {  /* else */
		   if(iscondition(++p)) { /*else if*/
			if(flg) {
				flg = 2;
				continue;
			}
			flg=chkstr(p,&x,&y,getdata);
		   }
		   else if(ff==1) flg ^= 1;
		   continue;
		}
		if(flg != 1) {
/* 逻辑为假，所有语句都不执行，但要看有条件语句的，嵌套计数器ff要计算一下 */
			if(iscondition(p)) ff++;
			continue;
		}
		if(iscondition(p)) {
/* flg=1时，遇见条件语句，按条件语句的逻辑值递归一层，并根据返回值修改flg */
			flg=do_dic(fd,chkstr(p,&x,&y,getdata));
			continue;
		}
		do_ss(p);
	}
	if(ferror(fd)) return errno;
	return 0;
}
static int iscondition(cp)
register char *cp;
{
char *p,*p1;
register int i;
	TRIMlf((unsigned char *)cp);
	i=strlen(cp);
	if(!i) return(0);
	if(cp[i-1]=='?') {
		cp[i-1]=0;
		return 1;
	}
	return 0;
}

static int true(p)/*判断某记录是否存在*/
char *p;
{
int i;
double x,y;
	i=chkstr(p,&x,&y,getdata);
        return(i);
}

int logic(p) /*将逻辑行以 ? : 为分隔符分解为算术表达式交
		do_ss 处理 */
char *p;
{
char part1[128],part2[256];
char *part3;
int ret;
		if(*p==';') {
			do_line(p);
			return(0);
		}
	if(*(part3=stptok(p,part1,sizeof(part1),"?"))){/*若有 ? */
		if(*part3 != '?') return((int)do_ss(p)); /* p too long */
		part3++;
		part3=stptok(part3,part2,sizeof(part2),":");
		if(*part3) part3++;
			/*part1   中放置 ? 前的串
			  part2   中放置 ? 与 : 之间的串
			  part3  所指的串存放 : 之后的串
			  */
		if (true(part1))
			do_line(part2);
		else   
			do_line(part3);			

	} else do_line(p);			

	return(0);

}

#define MAXREG 5
int do_ss(label)
char *label;
{
char *p,*p1,*sp,s[10],e[10];
int cc,i,j,k,start,end;
regmatch_t pmatch[MAXREG];
char *__loc1;
	if(!label || !*label) return 1;
	if(*label==';') {
		do_line(label);
		return 0;
	}
	if(!regexec(&reg_ss,label,MAXREG,pmatch,0)) { /* ,,,1..80=  */
		sp=label+pmatch[0].rm_eo;
		__loc1=label+pmatch[0].rm_so;

		p=malloc(end=(1+strlen(label)));
		p1=malloc(end);
		if(!p||!p1) {
			ShowLog(1,merr,"do_ss,1..80");
			return(1);
		}
		start=atoi(label+pmatch[1].rm_so);
		end=atoi(label+pmatch[2].rm_so);
		strcpy(p,sp);
		sprintf(__loc1,"%%d%s",p);
		strcpy(p,label);

		for(i=start;i<=end;i++) {
			sprintf(label,p,i);
			while(!regexec(&reg_ss1,label,MAXREG,pmatch,0)) {
				sp=label+pmatch[0].rm_eo;
				__loc1=label+pmatch[0].rm_so;
				k=atoi(label+pmatch[1].rm_so);
				j=k+i-start; 
				strcpy(p1,sp);
				sprintf(__loc1,"%%d%s",sp);
				strcpy(p1,label);

				sprintf(label,p1,j);
			}

			logic(label);
		}
		free(p);
		free(p1);
	} else logic(label);
	return 0;
}
static close_old_fd(fd)
FILE *fd;
{
	if(fd != stdout) {
		if(fd) fclose(fd);
		fd=0;
	}
	return errno;
}


unsigned char *TRIMlf(str)
unsigned char *str;
{
register int n=0;
register unsigned char *p;
	for (p=str;*p;n++) {
		if(*p=='#') {
			*p=0;
			break;
		}
		p++;
	}
	p--;
	while(n-- && (*p <= ' ') && (*p != '\f') && (*p != '\033')) {
		*p-- = 0;
	}
	return(str);
}
int ssinit()
{
	labn=10;
	return (
	((l=(double *)malloc(sizeof(double) * labn))!=0)&&
	(!regcomp(&reg_ss,ss_src,REG_EXTENDED)) &&
	(!regcomp(&reg_ss1,ss1_src,REG_EXTENDED)));
}
ssfree()
{
	l=(double *)myfree((char *)l);
	regfree(&reg_ss);
	regfree(&reg_ss1);
}
