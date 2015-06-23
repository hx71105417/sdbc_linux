#include <stdio.h>
main()
{
char buf[2048];
	while(!ferror(stdin)) {
		fgets(buf,sizeof(buf),stdin);
		if(feof(stdin)) break;
		buf[strlen(buf)-1]=0;
		printf("%s    %s\n",buf,buf);
	}
}
