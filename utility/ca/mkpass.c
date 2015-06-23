#include <datejul.h>
#include <des.h>
#include "dw.h"

char *keyfile="KEY.txt";
int proc(struct dwnode *np);
main(argc,argv)
int argc;
char *argv[];
{
FILE *fd;
unsigned char buf[256],devname[40];
char *passkey;
char md5[16];
DWS dwp;
int cc;
char salt[3],tmp[200];
int len;
u_int crc;
	tzset();
	if(argc>1) keyfile=argv[1];
	if(isatty(0)) fprintf(stderr,"Make Devices's Password\n");
    do {
	if(isatty(0)) {
		fprintf(stderr,"Input Device Name: ");
		fflush(stderr);
	}
	fgets(devname,sizeof(devname),stdin);
	if(feof(stdin)) break;
	TRIM(devname);
	if(!*devname) continue;
	crc=ssh_crc32(devname,strlen(devname));
	cc=initdw(keyfile,&dwp);
	if(cc>=0) {
		passkey=getdw(crc,&dwp);
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
	sprintf(buf,"%s%d%s",
		devname,getpid(),rusecstrfmt(tmp,now_usec(),"YYYYMMDDHH24MISSFF6"));
	len=strlen(buf);
	cc=ssh_crc32(buf,len);
	salt[0]=(cc&0x3f)+0x21;
	salt[1]=((cc>>6) & 0x3f)+0x21;
	salt[2]=0;
	MD5(buf,len,md5);
	byte_a64(tmp,md5,16);
	des_fcrypt(tmp,salt,buf);
	fprintf(fd,"#%s\n%u %s\n",devname,
		ssh_crc32(devname,strlen(devname)),buf),
	fclose(fd);
    } while(1);
	
	return 0;
}

int proc(struct dwnode *np)
{
	printf("%15d,%s\n",np->num,np->name);
	return 0;
}
