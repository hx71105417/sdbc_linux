#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <strproc.h>

extern int errno;
extern char LOGFILE[]; // showlog.c

int main(int argc,char *argv[])
{
char *buff;
char *p1,*p2;
char tmp[1000],dir[512],tmp1[2000];
FILE *fd;
int ret;
	if(argc>1) {
		p1=p2=0;
		if(isdir(argv[1])>0) strcpy(dir,argv[1]);
		else {
			p1=strrchr(argv[1],'/');
			if(p1) {
				*p1=0;
				strcpy(dir,argv[1]);
				*p1='/';
			} else strcpy(dir,".");
		}
		if(dir[strlen(dir)-1]=='/') dir[strlen(dir)-1]=0;
		buff=argv[1];
	} else buff=".",strcpy(dir,buff);
	sprintf(tmp,"ls %s",buff);
//	printf("cmd=%s\n",tmp);
	fd=popen(tmp,"r");
	if(!fd) {
		perror(tmp);
		return 1;
	}
	while((ret=fscanf(fd,"%s",tmp))==1) {
		p1=sc_basename(tmp);
		sprintf(tmp1,"%s/%s",dir,p1);
		if(isrfile(tmp1)<=0) continue; //not readble reg file
		puts(tmp1);
	}
	pclose(fd);
	return 0;
}
