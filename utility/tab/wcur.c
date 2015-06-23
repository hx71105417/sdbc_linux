#include <curses.h>
#define MIN(a,b) (((a)>(b))?b:a)
void	wcur(pwin,c,n)
WINDOW *pwin;
register char c;
register int n;
{
int y,x;
	y=pwin->_cury;
	x=pwin->_curx;
	n=MIN(pwin->_maxx-x+1,n);
	if(!n) return;
	while(n--) waddch(pwin,c);
	wmove(pwin,y,x);
}
