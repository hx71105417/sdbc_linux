#include <stdio.h>
#include <bignum.h>
#include <strproc.h>
#include <crc32.h>
main()
{
u_int pa[9];
char *cp="/fileserver/Develop/sdbc/send/BIRD";
char *p;
int i,CRC;
	CRC=ssh_crc32(cp,strlen(cp));
	printf("CRC=%08X,len=%d\n",CRC,strlen(cp)+1);
}
