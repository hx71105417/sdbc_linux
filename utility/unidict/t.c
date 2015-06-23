#include <stdio.h>

main()
{
FILE *fd;
char buf[2048];
	fd=popen("./chkrk yb4.chk <<!\n2003-07-30|5|yb4|-1|13|9|3|343|222|33|445|66|778|99|543|234|879|0||||\n!\n","r");
	if(!fd) {
		perror("chkrk");
		exit(1);
	}
	while(!ferror(fd)) {
		fgets(buf,sizeof(buf),fd);
		if(feof(fd)) break;
		fputs(buf,stdout);
	}
	pclose(fd);
	return 0;
}

