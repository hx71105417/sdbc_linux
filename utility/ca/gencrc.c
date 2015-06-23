#include <stdio.h>
main(int argc,char *argv[])
{
long CRC;
int i;
	if(argc>1) {
		for(i=1;i<argc;i++) {
			printf("%s,%04X\n",argv[i],
				0xFFFF&gencrc(argv[i],strlen(argv[i])));
		}
		return 0;
	}
	return 0;
}
