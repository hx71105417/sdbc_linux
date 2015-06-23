#include "xcom.h"
char *basename(char *);

/*  get filename from stdin,and send it to server */
int sendfile(T_Connect *conn)
{
FILE *fd;
char filename[512];
char sendfname[512];
char buffer[1024];
char *p;
int ret;
	while(!ferror(stdin)) {
		fgets(filename,sizeof(filename),stdin);
		if(feof(stdin)) break;
		TRIM((unsigned char *)filename);
		p=basename(filename);
		sprintf(sendfname,"$/%s",p);
//fprintf(stderr,"%s,%s\n",filename,sendfname);
		ret=N_Put_File(conn,filename,sendfname);
		if(ret) {
			sprintf(buffer,"Putfile %s %s err=%d",
				filename,sendfname,ret);
			ShowLog(1,buffer);
		} else {
			printf("%s\n",filename);
		}
	}
	return 0;
}
