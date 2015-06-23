#include <fdbc.h>
#include <unistd.h>

T_PkgType logview[]={
	{CH_CHAR,21,0,0,-1},{CH_CHAR,172},{CH_CHAR,21},{CH_CHAR,15},{-1,0}
};
void getca(char *ca)
{
	*ca=0;
	return;
}

clilogin(T_Connect *conn,T_NetHead *NetHead)
{

int cc;
struct log_s log_pack;
char *p;
char buf[256],pack[240]; // ca leng?
FILE *fd;
/*
	printf("Input zyrid: ");
	fflush(stdout);
	fflush(stdin);
	fgets(log_pack.zyrid,sizeof(log_pack.zyrid),stdin);
	TRIM((unsigned char *)log_pack.zyrid);
	printf("Input passwd: ");
	fflush(stdout);
	p=getpass("");
*/
	p=getenv("ZYRID");
	if(!p || !*p) {
		printf("Input zyrid: ");
		fflush(stdout);
		fgets(log_pack.zyrid,sizeof(log_pack.zyrid),stdin);
		TRIM((unsigned char *)log_pack.zyrid);
	} else {
		strcpy(log_pack.zyrid,p);
	}
	p=getenv("PASSWD");
	if(!p || !*p) {
		printf("Input passwd: ");
		fflush(stdout);
		p=getpass("getpass");
	} 
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
	sprintf(buf,"%s %s",p,pack);
printf("buf=%s\n",buf);
	NetHead->num=0;
	NetHead->data=buf;
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
