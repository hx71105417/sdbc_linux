#include <curses.h>
wdisp(dwin,str)
WINDOW *dwin;
register char *str;
{
	while (*str)  wco(dwin,*str++);
}
