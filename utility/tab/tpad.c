#include <curses.h>
#include <signal.h>
#include <panel.h>
#include <termio.h>

void quit(int n);
struct termio oldterm;
WINDOW *winbox(WINDOW *wp);

main(int argc,char *argv[])
{
WINDOW *wp,*wps,*wp1,*wp1s,*wpp;
int cc;
char *str1="TEST CURSES, Input: ";
PANEL *P1,*P2;
WINDOW *pad1,*pad2;
int strx,stry;
int begx,begy,endx,endy,h;
	ioctl(0,TCGETA,&oldterm);
	tzset();
	initscr();
	noecho();
	nonl();
	signal(SIGINT,quit);
	signal(SIGTERM,quit);
	signal(SIGHUP,quit);
	signal(SIGPWR,quit);
	stry=0,strx=0;
	begy=3,begx=5;
	endy=12,endx=30;
	pad1=newpad(50,160);
	if(!pad1) {
		move(3,10);
		printw("pad1 error!");
		refresh();
	}
wprintw(pad1,"abcdefghijklmnopqrstuvwxyzaaaaaaBBBBBBBBBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDDDDDDDDDDDDDklmnopqrstuvwxyzabcdefghijklmnopq");
wmove(pad1,2,0);
wprintw(pad1,"123456789012345678901234567890123456789022333333333333333333334444444444444444444455555555555555555555666666666777777777777777771234567890");
wmove(pad1,3,0);
wprintw(pad1,"abcdefghijklmnopqrstuvwxyzaaaaaaBBBBBBBBBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDDDDDDDDDDDDDklmnopqrstuvwxyzabcdefghijklmnopq");
wmove(pad1,4,0);
wprintw(pad1,"123456789011111111111112222222222222222222333333333333333333334444444444444444444455555555555555555555666666666777777777777777771234567890");
wmove(pad1,5,0);
wprintw(pad1,"abcdefghijklmnopqrstuvwxyzaaaaaaBBBBBBBBBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDDDDDDDDDDDDDklmnopqrstuvwxyzabcdefghijklmnopq");
wmove(pad1,6,0);
wprintw(pad1,"123456789011111111111112222222222222222222333333333333333333334444444444444444444455555555555555555555666666666777777777777777771234567890");
wmove(pad1,7,0);
wprintw(pad1,"123456789011111111111112222222222222222222333333333333333333334444444444444444444455555555555555555555666666666777777777777777771234567890");
wmove(pad1,8,0);
wprintw(pad1,"123456789012345678901234567890123456789022333333333333333333334444444444444444444455555555555555555555666666666777777777777777771234567890");
wmove(pad1,9,0);
wprintw(pad1,"abcdefghijklmnopqrstuvwxyzaaaaaaBBBBBBBBBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDDDDDDDDDDDDDklmnopqrstuvwxyzabcdefghijklmnopq");
wmove(pad1,10,0);
wprintw(pad1,"123456789011111111111112222222222222222222333333333333333333334444444444444444444455555555555555555555666666666777777777777777771234567890");
wmove(pad1,11,0);
wprintw(pad1,"abcdefghijklmnopqrstuvwxyzaaaaaaBBBBBBBBBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDDDDDDDDDDDDDklmnopqrstuvwxyzabcdefghijklmnopq");
prefresh(pad1,stry,strx,begy,begx,endy,endx);
	keypad(pad1,TRUE);
	cc=0;
	while(cc != oldterm.c_cc[VEOF]) {
		cc=wgetch(pad1);
		switch(cc) {
		case KEY_LEFT:
			if(strx>0) strx--;
			break;
		case KEY_RIGHT:
			h=endx-begx;
			if((pad1->_maxx-strx)>h) strx++;
			break;
		case KEY_UP:
			if(stry>0) stry--;
			break;
		case KEY_DOWN:
			h=endy-begy;
			if((pad1->_maxy-stry)>h) stry++;
			break;
		default:	break;
		}
		prefresh(pad1,stry,strx,begy,begx,endy,endx);
		
	}
	endwin();
	return 0;
}

void quit(n)
int n;
{
	endwin();
	exit(n);
}

WINDOW *winbox(WINDOW *wp)
{
WINDOW *subw;
int y,x;
	y=wp->_maxy-1;
	x=wp->_maxx-1;
	subw=derwin(wp,y,x,1,1);
	werase(wp);
	wborder(wp,ACS_VLINE,ACS_VLINE,ACS_HLINE,ACS_HLINE,
		ACS_ULCORNER,ACS_URCORNER,ACS_LLCORNER,ACS_LRCORNER);
/*
wmove(wp,4,2);
wprintw(wp,"sunwin:%d,%d",y,x);
*/
	return(subw);
}
