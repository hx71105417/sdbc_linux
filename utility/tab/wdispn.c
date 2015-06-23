#include <string.h>
#include "pcio.h"
#include <ctype.h>

void wdispn(winp,str,pos,n)
WINDOW *winp;
unsigned char *str;
int pos,n;
{
int i,m,y,x;
register unsigned char *p;
	getyx(winp,y,x);
	n=(m=winp->_maxx-x+1)<n?m:n;
	if(n<=0) return;
	i=strlen(str);
	if(i<=pos) {
		wcur(winp,' ',n);
	//	wclrtoeol(winp);
		return;
	}
	i-=(pos); /* max length of display */
	p=&str[pos];
	if(secondcc(str,p)) {
		waddch(winp,' ');
		i--,n--,p++;
	}
	if(i<=0 || n<=0) return;
	for(m=(i<=n)?i:n;m-- > 1;p++) {
		if(*p=='\007') {
			putp("\007");
			continue;
		}
		if(*p<' '&&!isspace(*p)) {
			waddch(winp,' '); 
			continue;
		}
		waddch(winp,*p);
	}
/*
getyx(winp,y,x);
move(20,0);
printw("pos=%d,n=%d,i=%d,maxx=%d,x=%d,*p=%02X",pos,n,i,winp->_maxx,x,*p);
refresh();
sleep(3);
*/
	if(firstcc(str,p)) waddch(winp,' ');
	else waddch(winp,(*p<=' ')?' ':*p);
	if(n>i) wcur(winp,' ',n-i);
}
