#include "pcio.h"
#include <term.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <strproc.h>


#define LSIZE 2048
#define PSIZE 80
#define PAGENUM 100
long ftell();
void intrb();
void quit();
void alm();

char tmpf[]="BTABXXXXXXX";
FILE *copytmp();

WINDOW *wv,*wh,*ww;
int atflg=0,aty,atx,hpos;
long atpos=0;

#define PS PSIZE*PAGENUM
long page[PS];
int maxline,pageline;
char *pp0[PSIZE],**p;
char *ph[24];

main(argc,argv)
int argc;
char *argv[];
{
int i,cc,flg;
FILE *_ttyin;
FILE *fd=stdin;
char *mktemp(),*getenv();
char *term;
SCREEN *scr;
	term=getenv("TERM");
	_ttyin=fopen("/dev/tty","r");
	if(!_ttyin) {
		printf("/dev/tty, Can not open Your tty!\n");
		quit(1);
	}
	scr=newterm(term,stdout,_ttyin);
	putp(init_2string);
	keypad(stdscr,TRUE);
	meta(stdscr,TRUE);
	noecho();
//	tmpf=tempnam("/tmp",tmpf);
	signal(SIGINT,intrb);
	signal(SIGHUP,intrb);
	signal(SIGTERM,intrb);
	signal(SIGPIPE,intrb);
	for(i=1;i<argc;i++) {
		if(*argv[i]=='-') {
			setflg(argv[i]+1);
			continue;
		}
		if(!(fd=fopen(argv[i],"r"))) {
			perror(argv[i]);
			sleep(3);
			continue;
		}

		cc=filedisp(fd);
		if(cc==Ctrl_D || cc==Esc) break;
		switch(cc&255) {
		case 'f':
		case 'F':
			i=2;break;
		case 'p':
		case 'P':
			flg=0;
			while(i>1) {
				i--;
				if(*argv[i]=='-') {
					if(!flg) {
						flg=1;
						continue;
					}
					else {
						break;
					}
				}
				else {
					if(flg&2) {
						i++;
						break;
					}
					else {
						flg|=2;
						continue;
					}
				}
			}
			i--;
			break;
		default: ;
		}
		fclose(fd);
	}
	if(fd==stdin) filedisp(fd);
	quit(0);
}

void intrb()
{
	quit(255);
}

void quit(n)
{
	move(LINES-1,0);
	refresh();
	endwin();
	unlink(tmpf);
	exit(n);
}

filedisp(fd)
FILE *fd;
{
int i,v,cc;
FILE *tfd;
char aa[LSIZE];
	if(!atflg) aty=atx=-1;
	tfd=copytmp(fd);
	pageline=0;
	erase();
	move(LINES-1,0);
	printw(" h j k l Retuen Space PgUp PgDn < >.  quit: e q ctrl_D");
	for(i=0;i<PSIZE;i++) pp0[i]=0;
	wv=wh=0;
	if(!tfd) {
		sprintf(aa,"can not open tmpf err= %d     ",errno);
		error(aa);
		return 0;
	}
/*
printf("atx=%d,COLS=%d,bad=%d\n",atx,COLS,COLS*4/5);
fflush(stdout);
sleep(3);
*/
	ww=newwin(LINES-aty-1,COLS-atx,aty,atx);
	if(!ww) {
		error("Can not create WINDOW ww !");
		return 0;
	}
	werase(ww);
	keypad(ww,TRUE);
	meta(ww,TRUE);
	if(atx|aty) {
	   if(atx) {
		wv=newwin(LINES-aty-1,atx,aty,0);
		if(!wv) {
			error("Can not create WINDOW wv !");
			return 0;
		}
	keypad(wv,TRUE);
	meta(wv,TRUE);
	werase(wv);
	    }
	    if(aty) {
		wh=newwin(aty,COLS-atx,0,atx);
		if(!wh) {
			error("Can not create WINDOW wh !");
			return 0;
		}
	keypad(wh,TRUE);
	meta(wh,TRUE);
	werase(wh);
	    }
	}
	for(i=0;i<aty;i++) {
		wmove(stdscr,i,0);
		if(ph[i]) wdispn(stdscr,ph[i],0,atx);
 		refresh();
	}
	hpos=atx;
	v=LINES-aty-1;
	for(i=0;i<v;i++) {
		if(!fgets(aa,sizeof(aa),tfd)) break;
		TRIM(aa);
		if(!(pp0[i]=strdup(aa))) {
			error("filedisp malloc err!");
			return 0;
		}
	}
	for(;i<v;i++) {
		pp0[i]=0;
	}
	refresh();
	cc=controlpage(tfd);
	if(wv) delwin(wv);
	if(wh) delwin(wh);
	delwin(ww);
	allfree();
	atflg=0;
	return(cc);
}


controlpage(fd)
FILE *fd;
{
int cc;
int y,x;
int i,v,h;
	v=LINES-aty-1;
	h=COLS-atx+1;
	y=x=0;
	do {
/*
printw("***** ABCDE   ***");
refresh();
sleep(2);
*/
		if(wh) {
			for(i=0;i<aty;i++) {
				wmove(wh,i,0);
				if(ph[i]) wdispn(wh,ph[i],hpos,h);
			}
			wrefresh(wh);
		}
		for(i=0;i<v;i++) {
		    if(wv) {
			wmove(wv,i,0);
			if(pp0[i]) wdispn(wv,pp0[i],0,atx);
		    }
			wmove(ww,i,0);
			if(pp0[i]) wdispn(ww,pp0[i],hpos,h);
			else wclrtoeol(ww);
			wmove(ww,y,x);
		}
		if(wv) wrefresh(wv);
		wrefresh(ww);
		cc=wgetch(stdscr);
		switch(cc) {
		case Ctrl_L:
		case Ctrl_R:	wrefresh(curscr); break;
		case KEY_UP:	y=wup(fd,1);break;
		case Return:
		case KEY_DOWN:	y=wdown(fd,1);break;
		case KEY_LEFT:	x=wleft(1);break;
		case KEY_RIGHT:	x=wright(1);break;
		case KEY_PPAGE:	y=wup(fd,(LINES-aty)/2);break;
		case KEY_NPAGE:	y=wdown(fd,(LINES-aty)/2);break;
		default:
			switch(cc&255) {
			case 'q':
			case 'Q':
			case 'E':
			case 'e':
			case 'p':
			case 'P':
			case 'f':
			case 'F':
				return(cc);
			case 'L':
			case 'l':
				hpos+=(COLS-atx)/2;
				break;
			case 'h':
			case 'H':
				if((hpos-=(COLS-atx)/2) < atx)
					hpos=atx;
				break;
			case ' ':
			case 'j':
			case 'J':
				y=wdown(fd,LINES-aty-2);break;
			case 'k':
			case 'K':
				y=wup(fd,LINES-aty-2);break;
			case 'm':
			case 'M':
				x=ww->_maxx/2;break;	
			case '$':
				x=ww->_maxx-1;break;	
			case '^':
				x=0;break;	
			default : ;
			}
		}
	}while(cc!=Ctrl_D && cc!=Esc);
	return(cc);
}

wup(fd,m)
FILE *fd;
int m;
{
int y,x;
int i;
char aa[LSIZE];
int n;
	getyx(ww,y,x);
	if(y-m >= 0) y-=m;
	else {
	    m-=y,y=0;
	    while(m-- && pageline>0) {
		pageline--;
		fseek(fd,page[pageline],0);
		if(fgets(aa,sizeof(aa),fd)) {
			n=LINES-1-aty;
			if(pp0[n]) free(pp0[n]);
			for(i=n;i>0;i--) 
				pp0[i]=pp0[i-1];
			TRIM(aa);
			pp0[0]=strdup(aa);
		}
	}}
	return y;
}
wdown(fd,m)
FILE *fd;
int m;
{
int y,x;
int i;
char aa[LSIZE];
int n;
	n=LINES-aty-1;
	getyx(ww,y,x);
	if(y<=ww->_maxy-m) {
		y+=m;
	}
	else { 
	   m-=ww->_maxy-y-1;
	   y=ww->_maxy;
	   while(m--) {
	   if(pageline<maxline-n) {
		fseek(fd,page[pageline + n],0);
		pageline++;
		if(fgets(aa,sizeof(aa),fd)) {
			free(pp0[0]);
			for(i=0;i<n-1;i++) 
				pp0[i]=pp0[i+1];
			TRIM(aa);
			pp0[i]=strdup(aa);
		}
	    }
	    else break;
	   }
	}
	if(y>=maxline-pageline) y=maxline-pageline-1;
	return y;
}
wright(m)
int m;
{
int n;
int y,x;
	getyx(ww,y,x);
	if(x<=ww->_maxx-m) x+=m;
	else  {
		hpos+=m;
		x-=m-1;
	}
	return x;
}

wleft(m)
int m;
{
int n;
int y,x;
	getyx(ww,y,x);
	if(x>=m) x-=m;
	else  {
		if((hpos-=m) >=atx) {
			;
		}
		else {
			hpos=atx;
			x=0;
		}
	}
	return x;
}


error(str)
char *str;
{
int y,x;
	getyx(stdscr,y,x);
	move(LINES-1,0);
	clrtoeol();
	wdisp(stdscr,str);
	refresh();
	sleep(3);
	move(y,x);
}

allfree()
{
int i;
	for(i=0;i<23;i++) {
		if(ph[i]) {
			free(ph[i]);
			ph[i]=0;
		}
	}
	for(i=0;i<PSIZE;i++) {
		if(pp0[i]) free(pp0[i]);
	}
}

FILE *
copytmp(fd)
FILE *fd;
{
int i,j;
FILE *fd1;
char aa[LSIZE],*cp;
	i=mkstemp(tmpf);
	if(i<0) {
		perror(tmpf);
		return 0;
	}
	fd1=fdopen(i,"w");
	if(!fd1) return 0;
	for (i=0;i<23;i++) ph[i]=0;
	signal(SIGALRM,alm);
	for (i=0;i<23;i++) ph[i]=0;
	signal(SIGALRM,alm);
	for(j=i=0;alarm(10),fgets(aa,sizeof(aa),fd);j++,i++) {
		alarm(0);
		if(j>=PS) break;
		if(atflg) {
			if(i<aty) {
				ph[i]=strdup(aa);
				j=-1;
			}
			else if(i==aty) {
				atpos=ftell(fd1);
				j=0;
			}
		}
		else if(i<(LINES*2/3) && i<23) {
		  if(aty<0) {
			cp=strchr(aa,'@');
			if(cp && (atx=cp-aa+1)<(COLS*2/3)) {
				j=-1;
				atpos=ftell(fd1)+strlen(aa),aty=i+1;
			}
			if(cp) *cp='+';
			ph[i]=strdup(aa);
		  }

		}
		if(j>=0) page[j]=ftell(fd1);
		fprintf(fd1,"%s",aa);
	}
	maxline=j;
	alarm(0);
	if(atx<0 || atx>(COLS*4/5)) atx=0;
	if(aty<0) aty=0;
	fclose(fd1);
	fd1=fopen(tmpf,"r");
	fseek(fd1,atpos,0);
	return(fd1);
}

void alm()
{
	signal(SIGALRM,alm);
}

setflg(str)
char *str;
{
	switch(*str) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		atflg=1;
		aty=atx=0;
		sscanf(str,"%d,%d",&aty,&atx);
		if(aty>(LINES*2/3) || atx>(COLS*4/5))
			atflg=0;
		else break;
	default:
		error("useage: batb [-y,x] [file] [-y,x] [file] ...");
		break;
	}
}
