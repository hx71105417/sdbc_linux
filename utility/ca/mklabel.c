#include <stdio.h>
#include <string.h>
#include <bignum.h>
#include <strproc.h>
#include <enigma.h>
#include <dw.h>

char *keyfile="KEY.txt";

main(argc,argv)
int argc;
char *argv[];
{
FILE *fd;
char buf[256],labelname[256];
char *passkey;
DWS dwp;
long tim;
char tmp[100];
int len,cc;
u_int crc;
//struct crypt_s cry;
ENIGMA2 egm;

//	*cry.salt=0;
	if(argc>1) keyfile=argv[1];
	fprintf(stderr,"Input Device Name: ");
	fflush(stderr);
	fgets(buf,sizeof(buf),stdin);
	TRIM(buf);
	crc=ssh_crc32(buf,strlen(buf));
//fprintf(stderr,"%s,crc=%u\n",buf,crc);
	cc=initdw(keyfile,&dwp);
	if(cc<0) {
		perror(keyfile);
		return 1;
	}
//for(cc=0;cc<dwp.alldw;cc++) 
//fprintf(stderr,"%u %s\n",dwp.node[cc].num,dwp.node[cc].name);
	passkey=getdw(crc,&dwp);
	if(!passkey) {
		fprintf(stderr,"Can not find %s %u in %s!\n",buf,crc,keyfile);
		freedw(&dwp);
		return 3;
	}
	fprintf(stderr,"name=%s,crc=%u,passkey=%s\n",buf,crc,passkey);
	fprintf(stderr,"Input label: ");
	fflush(stderr);
	fgets(labelname,sizeof(labelname),stdin);
	TRIM(labelname);
	len=strlen(labelname);
	enigma2_init(&egm,passkey,0);
	enigma2_encrypt(&egm,labelname,len);
	*buf=0;
	byte_a64(buf,labelname,len);
	printf("%s\n",buf);
	freedw(&dwp);
	return 0;
}

