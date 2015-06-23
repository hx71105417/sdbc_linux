#include <string.h>
#include <strproc.h>


#define ATTNUM 10
#define FLDN 1024

char *stptok(),*strndup();
char *attribute[ATTNUM];
void setattr(char *str); 
int attlen[ATTNUM];
void setattr(str)       /* make attribute table */
char *str;
{
int n,c,l,i;
char cbuf[1024],*cp,*p1,*strrchr();
	n=str[-2]-'0';
	if(n<0 || n>=ATTNUM) return;
	*cbuf=0;
	cp=cbuf;
	p1=str;
	l=0;
	do {
		p1=stptok(str,cp,(int)((cbuf+sizeof(cbuf))-cp),"\\");
		l+=(p1-str);
		cp+=(p1-str);
		if(!*p1||(cp>=cbuf+sizeof(cbuf))) break;
		p1++;
		switch(*p1) {
		case '\r':
		case '\n':
			   *p1=0;
		case 0:           
			   break;
		case 'E':
			  *cp++='\033';
			  *cp=0;
			  l++;
			  break;
		case 'n':
			  *cp++='\n';
			  *cp=0;
			  l++;
			  break;
		case 'r':
			  *cp++='\r';
			  *cp=0;
			  l++;
			  break;
		case 't':
			  *cp++='\t';
			  *cp=0;
			  l++;
			  break;
		case 'f':
			  *cp++='\f';
			  *cp=0;
			  l++;
			  break;
		case 'b':
			  *cp++='\b';
			  *cp=0;
			  l++;
			  break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			  for(i=0;i<3;i++) {
				*cp <<= 3;
				*cp += (*p1-'0');
				++p1;
				if(*p1<'0' || *p1>'7') break;
			  }
			  p1--;
			  cp++;
			  *cp=0;
			  l++;
			  break;
		case 'x':
			  p1++;
			  *cp=uabin(*p1)<<4;
			  p1++;
			  *cp+=uabin(*p1);
			  *++cp=0;
			  l++;
			  break;
		default:
			if(*p1) *cp++=*p1;
			*cp=0;
			  l++;
			break;
		}
		str=p1+1;
	} while(*p1);
	if(cp=strrchr(cbuf,'\n')) {
		*cp=0;
		cp--;
		if(*cp=='\r') *cp=0;
	}
	if(!attribute[n]) {
		attlen[n]=l-1;
		attribute[n]=strndup(cbuf,l);/* ATTNUL*/
	}
	return ;
}
uabin(chr)
unsigned char chr;
{
int c;
	c=chr-'0';
	if(c>9) c-=7;
	if(c>32) c-=32;
	return(c);
}
/*写文件并插入属性*/
void afputs(char *str,FILE *fd);    
void afputs(str,fd)
char *str;
FILE *fd;
{
char cbuf[FLDN];
int i;
	if(!str) return;
	TRIM(str);
	do {
		str=stptok(str,cbuf,sizeof(cbuf),"\033");
		fprintf(fd,"%s",cbuf);
		if(*str) {
			str++;
			switch(*str) {
			case 0: str--;break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				i=*str-'0';
				outattr(i,fd);
				break;
			default: fprintf(fd,"\033%c",*str);
			}
			str++;
		}
	}while(*str);
	fprintf(fd,"\n");
}
/*插入属性*/
putattr(str,insto)
char *str,**insto;
{
int i;
	if (*str != '\033') return(0);
	else {/*数据流(str)是属性字,插入到模板(insto)中*/
		TRIM(str);
		*insto=strsubst(*insto,0,str);
	}
	return(1);
}

outattr(i,fd)
int i;
FILE *fd;
{
char * cp;
int k;
	if(cp=attribute[i]) {
		for(k=0;k<attlen[i];k++) {
			fprintf(fd,"%c",*cp++);
		}
	}
}
