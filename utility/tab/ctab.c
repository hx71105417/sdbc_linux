/* %Z% %M% Ver %I% %D% */
/* ctab.c for LINUX */
/* New tab format */
/* ctab -i t.ctb
   attrib keeped */
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <libgen.h>
#include <regex.h>
#include <strproc.h>

#undef putchar
#undef getchar

#define ATTRNUM 10
#define MAXREG 5
struct tm day;
long today;
extern long timezone;
//extern char *malloc(),*strdup(int),*realloc(char *,int);
#define VARN 4
#define VALN 11
char *var[VARN]={
	"LSIZE=",
	"PSIZE=",
	"ATTRIBUTE[0-9]=",
	"INCLUDE="
};
char *attribute[ATTRNUM];
char *val[VALN]={
	"\\$DATE",
	"\\$TIME",
	"\\$y",
	"\\$m",
	"\\$d",
	"\\$H",
	"\\$M",
	"\\$PAGE",
	"\\$CWEEK",
	"\\$W",
	"\\$Y"
};
#define FLDN 1024
char bbuf[FLDN];
int  sepn,sepl;
char vari[]="%-{0,1}[0-9]+";
regex_t varip,var1p;
char var1[]="\\$-{0,1}[0-9]+";
regex_t  varp[VARN],valp[VALN];
regex_t  newp;
char *stptok();
char *buf;
char *seprateline;
char *tail;
int lsize=FLDN,psize=66;
int lineno=1;
int page;

FILE *tabfd,*tmpfd;
char tmpfilename[]="/usr/tmp/CTABXXXXXX";
char *mktemp(char *);
int TMPFILE;
FILE *inclopen(char *);

int genlen=0,genval=0,gensep=0; /* gensep:if seprateline it don't do */
long genpose,ftell();
#define CRGVN 10
int crgc;
char *crgv[CRGVN];

int leadn,tailn;
int glbval,leadval,tailval;
int	sflg,   /* ctab -s xxx */
	llineno, /* logic line No. */
	sline;  /* logic page seprate line number */
char *pp,*inclfile;
char *dir,*dirname(char *);
void do_onece();
char ctrlend[80];
char htmlflg=' ';

main(argc,argv)
int argc;
char *argv[];
{
int i,ret;
char *dirp,*p,*p1;
	crgc=1;
	sflg=0;
	pp=0;
	inclfile=0;
	leadn=tailn=0;
	glbval=leadval=tailval=0;
	*ctrlend=0;
	tzset();
	for(i=0;i<argc;i++) {
		if(*argv[i]=='-') {
			setflg(argv[i]+1);
		}
		else {
			if(crgc <= CRGVN) {
				crgv[crgc-1]=argv[i];
				crgc++;
			}
			else fprintf(stderr,"Too many args!\n");
		}
	}
	if(crgc>2) {
/* get dir name */
		dirp=strdup(crgv[1]);
		dir=dirname(dirp);
		for(i=0;i<VALN;i++) {
			ret=regcomp(&valp[i],val[i],REG_EXTENDED);
			if(ret) {
				char errbuf[200];
				regerror(ret,&valp[i],errbuf,sizeof(errbuf));
				fprintf(stderr,"mk%serr %s\n",val[i],errbuf);
				return 1;
			}
		}
		for(i=0;i<VARN;i++) {
			ret=regcomp(&varp[i],var[i],REG_EXTENDED);
			if(ret) {
				char errbuf[200];
				regerror(ret,&varp[i],errbuf,sizeof(errbuf));
				fprintf(stderr,"mk%serr %s\n",var[i],errbuf);
				return 1;
			}
		}
		ret=regcomp(&varip,vari,REG_EXTENDED);
		ret=regcomp(&var1p,var1,REG_EXTENDED);
		/* $n */
		ret=regcomp(&newp,"([+-]{0,1}[0-9]+)",REG_EXTENDED);
		today=time((long)0);
		day=*localtime(&today);
		TMPFILE=mkstemp(tmpfilename);
		if(TMPFILE<0) perror(tmpfilename);
		buf=bbuf;
		sepn=0;
		genval=0;
		if(crgc>3) 
			for(i=2;i<crgc-1;i++) {
				freopen(crgv[i],"r",stdin);   
				do_onece();
			}
		else do_onece();
	//	quit(0);
	}
	else {
		i=execvp("/bin/pr",argv);
		perror("execvp");
	}
//fprintf(stderr,"befor free valp and varp\n");
	for(i=0;i<VALN;i++)
		regfree(&valp[i]);
	for(i=0;i<VARN;i++) {
		regfree(&varp[i]);
	}
//fprintf(stderr,"after free valp and varp\n");
	regfree(&varip);
	regfree(&var1p);
	return 0;
}
void do_onece()
{
	tmpfd=fdopen(TMPFILE,"w");
	if(!tmpfd) {
		perror(tmpfilename);
		exit(1);
	}
	if(!(tabfd=fopen(crgv[1],"r"))){
		perror(crgv[1]);
		return;
	}
	readtab(tabfd);
	if(buf==bbuf) {
		buf=malloc(lsize);
	}
	fclose(tabfd);
	tabfd=freopen(tmpfilename,"r",tmpfd);
	if(!tabfd) {
		perror(tmpfilename);
		quit(1);
	}
	if(!sflg) 
		writetab(),(*ctrlend)?fputs(ctrlend,stdout):(sflg=sflg);
	else prtpara(),quit(0);
	fclose(tabfd);
}
char *c_week[]={
		"星期日",
		"星期一",
		"星期二",
		"星期三",
		"星期四",
		"星期五",
		"星期六"},
	*_week[]={
		"日",
		"一",
		"二",
		"三",
		"四",
		"五",
		"六"};
readtab(tabfd)
FILE *tabfd;
{
int i,j,rflg,genflg=0;
char cbuf[80],*cp,*incfile,ifile[270];
FILE *incfd;
regmatch_t matchs[MAXREG];
	while(fgets(buf,lsize,tabfd)) {
		rflg=0;
		if(*buf=='#') {
/***********************************************
 * WINDOS98 WORD Rich Text formaat head        *
 ***********************************************/
			if(!strncmp(buf+1,"CTRLHEAD",8)) {
				while(fgets(buf,lsize,tabfd)) {
					if(*buf=='#'&&!strncmp(buf+1,"CTRLEND",7))
						break;
					else if(!sflg) fputs(buf,stdout);
				}
	
			} else if(!strncmp(buf+1,"CTRLTAIL=",9)) {
/***********************************************
 * WINDOS98 WORD Rich Text formaat tail        *
 ***********************************************/
				strcpy(ctrlend,buf+10);
			}
			continue;
		}
		for(i=0;i<VALN;i++) {
			cp=buf;
			if(!regexec(&valp[i],cp,MAXREG,matchs,0))  {
//fprintf(stderr,"valp[%d]:%s\n",i,val[i]);
				switch(i) {
				case 0:
					sprintf(cbuf,"%02d/%02.2d/%02.2d",
						day.tm_year%100,
						day.tm_mon+1,
						day.tm_mday);
					break;
				case 1:
					sprintf(cbuf,"%02d:%02d",
						day.tm_hour,
						day.tm_min);
					break;
				case 2:
					sprintf(cbuf,"%02d",
						day.tm_year%100);
					break;
				case 3:
					sprintf(cbuf,"%02d",
						day.tm_mon+1);
					break;
				case 4:
					sprintf(cbuf,"%02d",
						day.tm_mday);
					break;
				case 5:
					sprintf(cbuf,"%02d",
						day.tm_hour);
					break;
				case 6:
					sprintf(cbuf,"%02d",
						day.tm_min);
					break;
				case 7:
					continue;
				case 8:
					sprintf(cbuf,"%s",
					   c_week[day.tm_wday]);
					break;
				case 9:
					sprintf(cbuf,"%s",
					   _week[day.tm_wday]);
					break;
				case 10:
					sprintf(cbuf,"%04d",
						day.tm_year+1900);
					break;
				}
				strncpy(cp+matchs[0].rm_so,cbuf,strlen(cbuf));
				cp=cp+matchs[0].rm_eo;
			}
		}
		for(i=0;i<VARN;i++) {
			cp=buf;
			while(!regexec(&varp[i],cp,MAXREG,matchs,0)) {
				cp=cp+matchs[0].rm_eo;
				rflg=1;
				switch(i) {
				case 0:
					if(buf==bbuf) {
						lsize=atoi(cp);
						buf=malloc(lsize);
						if(!buf) {
							perror("buf realloc");
							exit(1);
						}
					}
					continue;
				case 1:
					psize=atoi(cp);
					continue;
				case 2: /* attribute */
					setattr(cp);
					continue;
				case 3: /* INCLUDE */
					if(!(incfile=strtok(cp,"\n\r"))) {
						fprintf(stderr,
						"With out include file name!\n"
						  );
						 quit(1);
					}
					TRIM(incfile);
					incfd=inclopen(incfile);
					if(!incfd) {
						quit(1);
					}
				  	readtab(incfd);
				  	fclose(incfd);
				 	continue;
				}
			}
		}
		if(rflg) continue;
		i=serch(&var1p,buf,!sflg);
		j=serch(&varip,buf,0);
		if(*buf=='&') {
			cp=buf+1;
			if(isdigit(*cp)) sepn=atoi(cp);
			else sepn=1;
			while(isdigit(*cp)) cp++;
			seprateline=strdup(cp);
			continue;
		}
		if(*buf=='$') {
			tail=strdup(buf+1);
			continue;
		}
		if(*buf=='{') {
			if(!genlen) genflg=1;
		}
		else if(genflg && *buf=='}') {
			genflg=0;
			if(buf[1]!='\n'&& buf[1]!='\r' && buf[1]!='\0')
				genlen++,gensep=1;;
		}
		else {
			if(genflg) genlen++,genval+=j;
			else if(!genlen) leadval+=j,leadn++;
			else tailval+=j,tailn++;
			glbval+=i;
		}
		if(!sflg) fputs(buf,tmpfd);
		if(genflg && !genlen) { //记录循环体的开始点
			genpose=ftell(tmpfd);
		}
	}
	if(genflg ) {
		if(!sflg) fputs("}\r\n",tmpfd);
	}
}
writetab()
{
int cc;
int tflg,blkn=1;
char *cbuf,*p;
extern int attlen[ATTRNUM]; /* in setattr.c */
regmatch_t matchs[MAXREG];
int loopflg=0;
	if(inclfile) {
		if(*inclfile) {
		} else {
			attribute[0]="\0330";
			attribute[1]="\0331";
			attribute[2]="\0332";
			attribute[3]="\0333";
			attribute[4]="\0334";
			attribute[5]="\0335";
			attribute[6]="\0336";
			attribute[7]="\0337";
			attribute[8]="\0338";
			attribute[9]="\0339";
			for(cc=0;cc<ATTRNUM;cc++) attlen[cc]=2;
		}
	}
	if(!(cbuf=malloc(lsize))) {
		fprintf(stderr,"writetab:malloc cbuf err!");
		return;
	}
	page=1;
	tflg=0;
	*buf=0;
	for(lineno=1;
	   (cc=(int)fgets(buf,lsize,tabfd))||tflg;*buf=0) {
//printf("writetab:begin buf=%s\n",buf);
		if(*buf=='\n' || *buf=='\r') {
			fputs(buf,stdout);
			continue;
		}
		if(*buf=='}' && !loopflg) { //html
			fputs(buf,stdout);
			continue;
		}
		if(!cc) {
aaa:
/* while table=EOF inputfile is not EOF seek continue */
			if((cc=getchar())==EOF) {
				if(pp) putescline(pp);
				while(sline--) putchar('\n');
				break;
			}
			if(cc=='\f') {
				putchar(cc);
				fgets(cbuf,lsize,stdin);
				goto aaa;
			}
			else ungetc(cc,stdin);
			newpage(tflg);
			tflg=0;
			sline=4;
			continue;
		}
		if(*buf=='{') {  /* into loop bady */
			loopflg=1;
			continue;
		} else if(loopflg && *buf=='}') {/* exit loop bady */
			if((cc=getchar())!=EOF) {
				if(cc=='$') {
					chanpage();
					tflg=2; /*logic new page*/
					continue;
				}
				else if(cc=='&') {
					fgets(cbuf,lsize,stdin);
					if(*seprateline) {
						afputs(seprateline,stdout);
						blkn=1;
						sscanf(cbuf,"%d",&blkn);
						lineno++;
						llineno++;
					}
				}
				else ungetc(cc,stdin);
			}
			if(cc==EOF) {
				tflg=0;
				afputs(tail,stdout);
				continue;
			}
			if(fullpage(blkn)) {
				afputs(tail,stdout);
				tflg=1;
				continue;
			}
			else fseek(tabfd,genpose,0);
			dosep(buf+1);
		} else if(!regexec(&varip,buf,MAXREG,matchs,0)) { // in loop body
		/* \\ 原行照打,取决于数据,'\\'打头*/
			p=buf+matchs[0].rm_eo;
			cc=getchar();
			if(cc!='\\') { //nomol
				ungetc(cc,stdin);
				putline();
				continue; 
			}
			cc=getchar();
			if(cc=='\n' || cc=='\r') {
				if(cc=='\r') getchar();
				continue; /* 本行取消 */
			}
			ungetc(cc,stdin);
			fgets(cbuf,lsize,stdin);
			afputs(cbuf,stdout);
			lineno++;
			llineno++;
			/*
			fgets(buf,lsize,stdin);\* 数据行起字典作用 *\
			if(feof(stdin) || ferror(stdin)) continue;
			putline();
			*/
		} else {
		    while((cc=getchar()) == '\\') {
			fgets(buf,lsize,stdin);
			if(feof(stdin)) {
				cc=EOF;
				break;
			}
			afputs(buf,stdout);
			lineno++;
			llineno++;
		    } 
		    if(cc != EOF) ungetc(cc,stdin);
//printf("WRITEDB last:");
		    putline();
		    continue;
		}
	}
}
chanpage()  /* stdin: $5,3 */
{
char *cbuf;
	if(!(cbuf=malloc(lsize))) {
		fprintf(stderr,"chanpage malloc cbuf err!");
		return;
	}
	if(!fgets(cbuf,lsize,stdin)) {
		free(cbuf);
		return;
	}
	sline=0;
	if(pp) free(pp),pp=0;;
	pp=strdup(cbuf);   //pp=5,3
	psize=-abs(psize);
	if(tail) printf(tail);
	free(cbuf);
}
newpage(flg)
int flg; /* flg==0 noseek,==2 logic new page */
{
int i;
	   page++;
	   sepl=0;
	   if(flg) {
		   fseek(tabfd,0l,0);
		   if(flg==2) {
			if(sline) {
	   			for(i=0;i<sline;i++) putchar('\n');
				sline=0;
			}
			else if (pp) {
				putescline(pp);
//ShowLog(5,"newpage free(pp)");
				free(pp);
				pp=0;
			}
			flg=0;
		   }
	   }
	   if(psize>0) printf("\f\n");
	   else for(i=0;i<sline;i++) putchar('\n');
	   psize=abs(psize);
	   lineno=1;
}
putescline(str)
char *str;
{
int i;
char *p;
	if(!str) return;
	while(*str) {
		switch (*str) {
		case '\\':
			str++;
			switch(*str) {
			case 0:return;
			case 'n':
				fputc('\n',stdout);
				break;
			case 'r':
				fputc('\r',stdout);
				break;
			case 'f':
				fputc('\f',stdout);
				break;
			case '\\':
				fputc('\\',stdout);
				break;
			default:
				fputc(*str,stdout);
				break;
			}
			break;
		case '\033':
			str++;
			i=  *str-'0';
			outattr(i,stdout);
			break;
		default:
			if(isdigit(*str)) { //str=pp=5,3 ... 3
				i=(int)strtol(str,&p,10);
				if(p != str) {
					while(i--) fputc('\n',stdout);
					str=p;
				}
				else fputc(*str,stdout);
			}
			else fputc(*str,stdout);
			break;
		}
		str++;
	}
}
putline()
{
char *cp,cbuf[10];
regmatch_t match[MAXREG];
char *__loc1;
	cp=buf;
	while(!regexec(&valp[7],cp,MAXREG,match,0)) { //$PAGE
		__loc1=cp+match[0].rm_so;
		cp+=match[0].rm_eo;
		sprintf(cbuf,"%5d",page);
		strncpy(__loc1,cbuf,strlen(cbuf));
	}
	serch(&varip,buf,1);
	afputs(buf,stdout);
	lineno++;
}
serch(ex,p,flg)
regex_t *ex;
char *p;
int flg;
{
int n,i,j,k;
char *cp,*ep,*aa,*cbuf;
regmatch_t match[MAXREG];
char *__loc1;
	if(!p||!ex) return(0);
	if(!(cbuf=malloc(lsize))) {
		fprintf(stderr,"malloc cbuf err!");
		return 0;
	}
	if(!(aa=malloc(lsize))) {
		fprintf(stderr,"malloc aa err!");
		free(cbuf);
		return 0;
	}
	n=0;
	while(!regexec(ex,p,MAXREG,match,0)) {
		__loc1=p+match[0].rm_so;
		p += match[0].rm_eo;
	   if(flg) {
		i=0;*cbuf=0;
		j=sscanf(__loc1+1,"%d",&i);
		if(j==1) {
		  for(ep=p;*ep==htmlflg;ep++) ;
		  j=ep-__loc1;
		  if((k=abs(i))<2 || j<k) i=(i>=0)?j:-j;
		  for(cp=__loc1;cp<p;cp++) *cp=' ';
		  do { /* get term */
	  		fgets(cbuf,lsize,stdin);
	  	  } while(putattr(cbuf,&__loc1));
		  cp=strrchr(cbuf,'\n');
		  if(cp) {
			*cp=0;
			if(cp[-1]=='\r') cp[-1]=0;
		  }
/*
		  cp=strchr(cbuf,'\033');
		  if(cp) *cp = 0;
*/
		  if(abs(i) < (k=strlen(cbuf))) {
			for(j=0;j<k;j++) cbuf[j]='?';
			cbuf[j]=0;
		  }
		  sprintf(aa,"%*.*s",i,abs(i),cbuf); 
		  strncpy(__loc1,aa,strlen(aa));
/* html不认识空格，替换后，所有空格都要变成&nbsp; */
		  if(htmlflg!=' ') {
			for(cp=__loc1;cp<ep;cp++) {
				if(*cp==' ') {
					cp=strsubst(cp,1,"&nbsp;")-1;
					ep += 5; 
				}
			}
		  }
		} else if(__loc1[1]=='I') {  /* %I */
/* %I 进行不限制宽度的填充 */
		}
/*
		for(p=ep;
			ep ;ep=strchr(++ep,'\033')) {
				p=strsubst(p,0,ep);
		}
*/
		p=ep;
	    }
	    n++;
	}
	free(aa);
	free(cbuf);
	return(n);
}
dosep(str)
unsigned char *str;
{
int cc;
	if((cc=getchar()) == EOF) return;
	ungetc(cc,stdin);
	if(cc=='$') return;
	lineno++;
	if(seprateline && sepn && !((++sepl)%sepn))
		afputs(seprateline,stdout);
	else if(gensep) afputs(str,stdout);
	else {
		lineno--;
	}
}
setflg(s)
char *s;
{
	switch(*s) {
	case 'S':
	case 's':
		sflg=1;
		break;
	case 'i':
		inclfile=s+1;
		break;
	case 'h':	//html
		htmlflg='-';
		psize=0;
		break;
	default: return(1);
/*
		fprintf(stderr,
		    "userage: ctab tabfile <inputfile >outputfile\n");
		quit(1);
*/
	}
	return 0;
}

prtpara()
{
int i;
	printf("Page size is:  %d\n",psize);
	printf("Line length is:  %d\n",lsize);
	printf("Globle var=%d\n",glbval);
	printf("Lead var=  %d\n",leadval);
	printf("Tail var=  %d\n",tailval);
	printf("Loop group:\n\t%d Lines \n\t%d var\n",genlen,genval);
	i=(abs(psize)-leadn-tailn-3);
	if(tail) i--;
	printf("\ttab body line=  %d\n",i);
	i+=gensep;
	i=sepn?
	(int)(0.5+(double)i*sepn/((double)genlen*sepn+1-gensep)):
	genlen?i/genlen:i;
	printf("\tloop group  per page=  %d\n",i);
	printf("\ttab var=  %d\n",i*=genval);
	printf("\tvar per page=  %d\n",i+=leadval+tailval);
}
fullpage(blkn)
int blkn;
{
int no;
	no=lineno+tailn+genlen*blkn+1-gensep;
	if(sepn && !((sepl+1)%sepn)) no++;
	if(tail) no++;
	if(psize>(leadn+tailn) && no>=abs(psize)) {
		return(1);
	}
	else return(0);
}
quit(n)
{
	fclose(tabfd);
	unlink(tmpfilename);
	exit(n);
}
FILE *inclopen(incfile)
char *incfile;
{
FILE *incfd;
char ifile[256];
	if(!incfile) return 0;
	if(dir && *incfile != '/' && *incfile!='.') {
		sprintf(ifile,"%s/%s",dir,incfile);
	} else {
		strcpy(ifile,incfile);
	}
	incfd=fopen(ifile,"r");
	if(!incfd) perror(ifile);
	return incfd;
}
