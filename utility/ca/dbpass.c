#include <stdio.h>
#include <net.h>
#include "dw.h"
#include <crc32.h>
char *crypt();
char *getsalt(char *salt);

char *keyfile="KEY.txt";
int proc(struct dwnode *np);
main(argc,argv)
int argc;
char *argv[];
{
FILE *fd;
unsigned char buf[256],devname[16],crc32[100];
char *passkey;
char aa[100];
DWS dwp;
int cc;
long tim;
char *cryptp,salt[3],tmp[100];
int len;
u_int crc;
	if(argc>1) keyfile=argv[1];
	fprintf(stderr,"Make Devices's Password\n");
do {
	fprintf(stderr,"Input Device Name: ");
	fflush(stderr);
	fgets(devname,sizeof(devname),stdin);
	if(feof(stdin)) break;
	TRIM(devname);
	if(!*devname) continue;
	crc=ssh_crc32(devname,strlen(devname));
	cc=initdw(keyfile,&dwp);
	if(cc>=0) {
		passkey=getdw(crc,&dwp);
//	BB_Tree_Scan(dwp.node,proc);
		freedw(&dwp);
		if(passkey) {
			fprintf(stderr,"%s already in %s!\n",devname,keyfile);
			continue;
		}
	}
	fd=fopen(keyfile,"a");
	if(!fd) {
		perror(keyfile);
		return 4;
	}

	getsalt(salt);
	strcpy(buf,devname);
	cc=ssh_crc32(buf,strlen(buf));
	sprintf(crc32,"%08X",cc);
	strcat(buf,crc32);
	sprintf(tmp,"%x%s",crc,devname);
	cryptp=des_fcrypt(tmp,salt,(char *)crc32);
	strcpy(aa,crypt(tmp,salt));
//fprintf(stderr,"fcrypt=%s,crypt=%s\n",cryptp,aa);
//	crc=ssh_crc32(devname,strlen(devname));
	fprintf(fd,"#%s\n%u %s\n",devname,crc,cryptp);
	fclose(fd);
} while(1);
	
	return 0;
}

int proc(struct dwnode *np)
{
	printf("%15d,%s\n",np->num,np->name);
	return 0;
}
