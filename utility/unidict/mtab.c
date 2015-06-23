#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#define MAXFILE 16
#define MAXDUP  10

struct stacks {
	int cont;
	long pos;
	}stack[MAXDUP],*sp;
FILE *ifile[MAXFILE],*tabfd=0;
int ifn=1;
long loop=-1,ftell();
int endflg=0;
void getflg(),putterm(),filecopy(),pop();
#define FLDN 2048

main(argc,argv)
int argc;
char *argv[];
{
int i,j;
char aa[FLDN],*p;
	ifile[0]=stdin;
	for(i=1;i<argc;i++) {
		if(*argv[i]=='-') {
			getflg(argv[i]+1);
			continue;
		}
		if(ifn<MAXFILE) {
			ifile[ifn]=fopen(argv[i],"r");
			ifn++;
		}
		else {
			fprintf(stderr,"Too many file,MAXFILE is %d\n",
				MAXFILE-1);
			exit(1);
		}
	}
	if(!tabfd) {
		if(ifn>1) for(i=1;i<ifn;i++) filecopy(ifile[i]);
		else filecopy(stdin);
	}
	else {
		initstack();
		while(1) {
			p=fgets(aa,sizeof(aa),tabfd);
			if(feof(tabfd)) {
				if(endflg) {
					for(i=0;i<ifn;i++) {
					  if(!(endflg & (1<<i))) {
						continue;
					  }
					  if((j=fgetc(ifile[i])) != EOF)
						ungetc(j,ifile[i]);
					  else endflg &= ~(1<<i);
					}
				}
				if(!endflg) loop=-1;
				if(loop==-1) break;
				endflg=0;
				clearerr(tabfd);
				fseek(tabfd,loop,0);
				continue;
			}
			if(*aa=='#') continue;
			if(*aa=='{') {
				push(aa+1);
				continue;
			}	
			if(*aa=='}') {
				pop();
				continue;
			}
			if(*aa=='%') {
				if(isdigit(aa[1])) {
					j=1;
					sscanf(aa+1,"%d,%d",&i,&j);
					putterm(i,j,!strchr(aa+1,'+'));
				}
				else initstack(),loop=ftell(tabfd);
				continue;
			}
			if(*p=='\\') p++;
			fputs(p,stdout);
		}
	}
	return 0;
}

void getflg(str)
char *str;
{
	switch(*str) {
	case 'f':
	case 'F':
		tabfd=fopen(str+1,"r");
		if(!tabfd) {
			perror(str+1);
			exit(1);
		}
		return;
	default:
		fprintf(stderr,
		   "mtab [-ftabfile] [f1 f2 ... fMAX] <f0 >outputfile");
		exit(2);
	}
}

void filecopy(fd)
FILE *fd;
{
char aa[FLDN];
	if(!fd) return;
	while(fgets(aa,sizeof(aa),fd))
		fputs(aa,stdout);
}
void  putterm(i,j,mode)
 int i,j;
 int mode;
 {
char aa[FLDN],*cp;
	if(i>ifn || !ifile[i]) {
		while(j--) putchar('\n');
		return;
	}
	while(j--) {
		(cp=fgets(aa,sizeof(aa),ifile[i]));
		if(feof(ifile[i])) {
			break;
		}
		if(*cp=='\033' && cp[1]>='0' && cp[1]<='9') {
			fputs(cp,stdout);
			j++;
			continue;
		}
		if(cp) endflg|=(1<<i);
		else endflg &= ~(1<<i);
		if(!mode) continue;
		if(feof(ifile[i])) {
			do{
			 putchar('\n');
			}while(j--);
			fclose(ifile[i]);
			ifile[i]=0;
			return;
		}
		fputs(aa,stdout);
	}
}
push(str)
char *str;
{
	if(sp-stack<MAXDUP) {
		sp->cont=1;
		sscanf(str,"%d",&sp->cont);
		sp->pos=ftell(tabfd);
	}
	else {
		fprintf(stderr,"Loop over flow %d\n",
			sp-stack-MAXDUP+1);
	}
	sp++;
}
void pop()
{
	if(sp<=stack) return;
	sp--;
	if(sp-stack < MAXDUP) {
		if(--sp->cont) {
			fseek(tabfd,sp->pos,0);
			sp++;
		}
	}
}
initstack()
{
	sp=stack;
	sp->cont=0;
	sp->pos=0;
}
