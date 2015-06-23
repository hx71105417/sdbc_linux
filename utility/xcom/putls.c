#include <libgen.h>
#include <sys/types.h>
//#include <sdbc.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include "xcom.h"

extern int substitute_env(char *line);

char *strrtok(char *src,char *beg,char *tok);
int sh2reg(char *str);
void putlist(T_Connect *,char *str);
extern int NZ_Put_File(T_Connect *conn,char *local,char *remote);

/* str:
	local_filename remodt_filename
*/
void putlist(T_Connect *conn,char *str)
{
char lpath[512],*dir,*rmtp;
char *filename;
DIR *dirp;
struct dirent *direp;
regex_t reg;
regmatch_t pmatch[2];
int ret;
char errbuf[1024],rmtbuf[1024];
	TRIM(str);
	if(!*str) return;
	rmtp=stptok(str,lpath,sizeof(lpath)," 	");
	substitute_env(lpath);
	if(*rmtp) rmtp=skipblk(rmtp);
	if(!*rmtp) rmtp="$/";
//	filename=basename(lpath);
	filename=strrchr(lpath,'/');
	if(filename) {
		*filename++=0;
		dir=lpath;
		if(!*dir) dir="/";
		if(!*filename) strcpy(filename,"*");
	} else {
		filename=lpath;
		dir=".";
	}

ShowLog(5,"dir=%s,name=%s:\n",dir,filename);
	ret=sh2reg(filename);
	if(0!=(ret=regcomp(&reg,filename,REG_NOSUB))) {
		regerror(ret,&reg,errbuf,sizeof(errbuf));
		ShowLog(1,"filename %s regcomp err=%s",filename,errbuf);
		return;
	}
	if(!(dirp=opendir(dir))) {
		ShowLog(1,"putls opendir %s err %d",dir,errno);
		regfree(&reg);
		return;
	}
	while(NULL != (direp=readdir(dirp))) {
		strcpy(errbuf,dir);
		ret=regexec(&reg,direp->d_name,1,pmatch,0);
		if(!ret) {
/* send file to server */
			if(dir[strlen(dir)-1]=='/')
				 sprintf(errbuf,"%s%s",dir,direp->d_name);
			else
				 sprintf(errbuf,"%s/%s",dir,direp->d_name);
			if(isrfile(errbuf)>0){
				if(rmtp[strlen(rmtp)-1]=='/') 
				    sprintf(rmtbuf,"%s%s",
					rmtp,basename(direp->d_name));
				else sprintf(rmtbuf,"%s/%s",
					rmtp,basename(direp->d_name));
				ret=NZ_Put_File(conn,errbuf,rmtbuf);
				if(ret) ShowLog(1,"putlist:%s %s,ret=%d",
					errbuf,rmtbuf,ret);
				else {
					printf("%s\n",errbuf);
					ShowLog(2,"NZ_PutFile %s to %s succeed!",
						errbuf,rmtbuf);
				}
				if(ret<0) break;
			}
		}
	}
/*
	if(errno) {
		ShowLog(1,"putlist %s err=%d",errbuf,errno);
	}
*/
	
	closedir(dirp);
	regfree(&reg);
	return;
}

int sh2reg(char *str)
{
char *p,*p1;
int flg=0;
	if(!str) return flg;
	strins(str,'^');
	for(p1=p=str; *p ; p++) {
		p=stptok(p,0,0,"?*.[\\");
		if(!*p) break;
		if(*p=='\\') {
			++p;
			continue;
		}
		if(*p=='[') {
			p1=strchr(p,']');
			if(p1) {
				flg=1;
				p=p1;
				continue;
			}
		}
		switch(*p) {
		case '?':
			*p='.';
			flg=1;
			break;
		case '*':
			p=strins(p,'.');
			flg=1;
			break;
		case '.':
			p=strins(p,'\\');
			break;
		default:
			break;
		}
	}
	strcat(str,"$");
	return flg;
}

char *strrtok(src,beg,tok)
char *src,*beg,*tok;
{
register unsigned char *p,*p1=0;
	p=(unsigned char *)src;
	if(!beg || !p||!*p) return 0;
	if(tok && *tok) for( ;p>=(unsigned char *)beg && *p;p--) {
		p1=(unsigned char *)strchr(tok,*p);
		if(p1) {
			p1=p;
			break;;
		}
	}
	return (char *)p1;
}

