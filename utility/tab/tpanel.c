#include "pcio.h"
#include <signal.h>
#include <panel.h>
#include <termio.h>

void quit(int n);
struct termio oldterm;
WINDOW *winbox(WINDOW *wp);

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
// wmove(wp,4,2);
// wprintw(wp,"sunwin:%d,%d",y,x);
	return(subw);
}

// #define wcbox winbox
main(int argc,char *argv[])
{
WINDOW *wp,*wps,*wp1,*wp1s,*wpp;
int cc,len;
char *str1="TEST CURSES, Input: ",aa[256];
PANEL *P1,*P2;
chtype ch;
	ioctl(0,TCGETA,&oldterm);
	tzset();
	len=strlen(str1);
	strcpy(aa,str1);
	initscr();
	noecho();
	nonl();
	signal(SIGINT,quit);
	signal(SIGTERM,quit);
	signal(SIGHUP,quit);
	signal(SIGPWR,quit);
	erase();
	move(1,10);
	printw("This is MAIN WINDOW named 'stdscr'.");
	refresh();
	wp1=newwin(6,30,8,10);
	wp=newwin(6,30,10,20);
	if(!wp) {
		move(LINES-2,0);
		printw("    newwin error!    ");
		refresh();
		getch();
		quit(5);
	}
	wp1s=wcbox(wp1);
	wrefresh(wp1);
	wmove(wp1s,0,0);
//	wdispn(wp1s,"This WINDOW WP1s! long long long long LINES",6,15);
	waddstr(wp1s,"This WINDOW WP1s! long long long long LINES");
	wmove(wp1s,3,1);
	wprintw(wp1s,"Be Over by another WONDOW!");
	wrefresh(wp1s);
	wps=wcbox(wp);
	wrefresh(wp);
	if(!wps) {
		move(LINES-2,0);
		printw(" wps   newwin error!    ");
		refresh();
		getch();
		quit(5);
	}
	intrflush(wp1s,FALSE);
	keypad(wp1s,TRUE);
	intrflush(wps,FALSE);
	keypad(wps,TRUE);
	wmove(wps,0,0);
	wprintw(wps,"This WINDOW 'wps'");
	P2=new_panel(wp1);
	P1=new_panel(wp);
	wpp=wps;
	while(wmove(wpp,2,0),
	   wprintw(wpp,"%s ",str1),
//	   wrefresh(wpp),
	   update_panels(),
	   (cc=waccept(wpp,-1,-1,aa,len))!=-1) {
	   	if(cc=='\f') {
//			wrefresh(curscr);
			wrefresh(wpp);
			update_panels();
			doupdate();
		}
	    	else {
			wmove(wpp,3,0);
			wattron(wpp,A_STANDOUT);
			wprintw(wpp,"Input=%04X  ",cc);
			wattroff(wpp,A_STANDOUT);
			wmove(wpp,3,8);
			ch=winch(wpp);
			wmove(wpp,3,11);
			wprintw(wpp,"%08X",ch);
	   	}
	   	if(cc==oldterm.c_cc[VEOF]) break;
		else if(cc==KEY_NPAGE) {
			if(wpp==wps) {
				wpp=wp1s;
				top_panel(P2);
			}
			else {
				wpp=wps;
				top_panel(P1);
			}
		}
	}
	wrefresh(wpp);
	del_panel(P2);
	del_panel(P1);
	delwin(wps);
	delwin(wp);
	delwin(wp1s);
	delwin(wp1);
	endwin();
	return 0;
}

void quit(n)
int n;
{
	endwin();
	exit(n);
}

