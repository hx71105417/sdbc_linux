#include "xcom.h"

void getca(char *ca)
{
	*ca=0;
	return;
}

clilogin(T_Connect *conn,T_NetHead *NetHead)
{

int cc;
char *p;
char buf[256],pack[240]; // ca leng?
FILE *fd;
/*
	fprintf(stderr,"Input zyrid: ");
	fflush(stderr);
	fflush(stdin);
	fgets(log_pack.zyrid,sizeof(log_pack.zyrid),stdin);
	TRIM((unsigned char *)log_pack.zyrid);
	p=getpass("Input password:");
	strcpy(log_pack.zyrpasswd,p);
	p=getenv("LABEL");
	if(!p||!*p) {
		fprintf(stderr,"env LABEL not found!\n");
		return 1;
	}
	fd=fopen(p,"r");
	if(!fd) {
		perror(p);
		return 2;
	}
	fgets(log_pack.label,sizeof(log_pack.label),fd);
	fclose(fd);
//fprintf(stderr,"LABEL=%s\n",log_pack.label);
	TRIM((unsigned char *)log_pack.label);
	getca(log_pack.ca);
	p=getenv("DEVNAME");
	if(!p||!*p) {
		fprintf(stderr,"env DEVNAME not found!\n");
		return 1;
	}
	net_pack(pack,(char *)&log_pack,logview);
	strcpy(gp->devname,p);
	sprintf(buf,"%s %s",p,pack);
fprintf(stderr,"buf=%s\n",buf);
*/
	NetHead->num=0;
	NetHead->data="";
	NetHead->pkg_len=strlen(NetHead->data)+1;
	NetHead->errno1=0;
	NetHead->errno2=0;
	NetHead->e_node=0;
	NetHead->d_node=0;
	p=getenv("OBJNO");
	if(p&&*p) NetHead->d_node=atoi(p);
	cc=SendPack(conn,NetHead);
	if(cc) {
		fprintf(stderr,"send %d\n",cc);
		return cc;
	}
	cc=RecvPack(conn,NetHead);
/*  get gw */
	return cc;
}
