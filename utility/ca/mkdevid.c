#include <stdio.h>
#include <string.h>
#include <bignum.h>
#include <strproc.h>
#include <strproc.h>
#include <enigma.h>
#include <dw.h>

char *keyfile="KEY.txt";
char *mkpassf="mkpass.in";

main(argc,argv)
int argc;
char *argv[];
{
FILE *fd,*mkpassfd;
char *p,buf[256],devfile[28],labelname[256],devid[20];
char *passkey,*dirp;
DWS dwp;
long tim;
char tmp[100];
int len,cc;
u_int crc;
ENIGMA2 egm;

	if(argc>1) keyfile=argv[1];
	cc=initdw(keyfile,&dwp);
	if(cc<0) {
		perror(keyfile);
		return 1;
	}
	dirp=getenv("CADIR");
	if(!dirp || !*dirp) dirp=".";
	mkpassfd=fopen(mkpassf,"a");
	if(!mkpassfd) perror(mkpassf);
	while(!feof(stdin) && !ferror(stdin)) {
		fgets(buf,sizeof(buf),stdin);
		if(feof(stdin)) break;
		TRIM(buf);
		p=skipblk(buf);
		if(!*p || *p=='#') continue;
		sscanf(buf,"%s%s",devfile,labelname);
		stptok(devfile,devid,sizeof(devid),".");
		crc=ssh_crc32(devid,strlen(devid));
		passkey=getdw(crc,&dwp);
		if(!passkey) {
		    if(mkpassfd) fprintf(mkpassfd,"%s\n",devid);
		    continue;
		}
		len=strlen(labelname);
		enigma2_init(&egm,passkey,0);
		enigma2_encrypt(&egm,labelname,len);
		byte_a64(buf,labelname,len);
		sprintf(tmp,"%s/%s",dirp,devfile);
		fd=fopen(tmp,"w");
		if(!fd) {
			perror(tmp);
			continue;
		}
		fprintf(fd,"%s\n",buf);
		fclose(fd);
		crc=0xFFFF&gencrc(devfile,strlen(devfile));
		printf("文件名:%s 校验码:%04X\n",devfile,crc);
	}
	freedw(&dwp);
	if(mkpassfd) fclose(mkpassfd);
	return 0;
}

