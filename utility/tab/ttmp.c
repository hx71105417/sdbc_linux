#include <stdio.h>
#include <stdlib.h>
main()
{
	char *templet="BTABXXXXXXX";
	char *tmpf;
	tmpf=tempnam("/tmp",templet);
	printf("%s\n",tmpf);
	return 0;
}
