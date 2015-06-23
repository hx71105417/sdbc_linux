#include "pcio.h"
#undef putchar
#define _FIRSTCC	NCURSES_BITS(1UL,31)
void wco(pwin,c)
register unsigned c;
register WINDOW *pwin;
{
int y,x,flg;
	c&=255;
	if(c=='\007'){
		putchar(c),fflush(stdout);
		return;
	}
	y=pwin->_cury;
	x=pwin->_curx;
	if(c == '\r') {
		x=0;
		wmove(pwin,y,x);
		return;
	}
	flg=iscc(c);
	if(x==pwin->_maxx-1) {
		if(pwin->_flags&_FIRSTCC && flg) waddch(pwin,c),c=' ';
		else waddch(pwin,' ');
		x=0;y++;
		wmove(pwin,y,x);
	}
	waddch(pwin,c);
	if(flg) pwin->_flags ^= _FIRSTCC;
	else pwin->_flags &= ~_FIRSTCC;
}
