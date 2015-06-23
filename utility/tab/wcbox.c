#include "pcio.h"
WINDOW *wcbox(win)
WINDOW *win;
{
int i;
int b=0,e=1;
WINDOW *sw;
    for(i=0;i<win->_maxy;i++){
    	wmove(win,i,0);
	waddnstr(win,"                                                                                 ",win->_maxx);
    }
    if(win->_maxy==1){
    	wmove(win,0,0);
    	waddstr(win,"[");
    	wmove(win,0,win->_maxx-2);
    	waddstr(win,"]");
    	sw=derwin(win,win->_maxy,win->_maxx-1,0,1);
    	return(sw);
    }
    if(win->_maxy==2){
    	wmove(win,0,0);
    	waddstr(win,"©°");
    	wmove(win,0,win->_maxx-2);
    	waddstr(win,"©´");
    	wmove(win,1,0);
    	waddstr(win,"©¸");
    	wmove(win,1,win->_maxx-2);
    	waddstr(win,"©¼");
    	sw=derwin(win,win->_maxy,win->_maxx-1,0,1);
    	return(sw);
    }
    if(win->_maxx<6){
    	box(win,'|','-');
    	sw=derwin(win,win->_maxy-1,win->_maxx-1,1,1);
    	return(sw);
    }
    if(ena_acs){
//	    wattron(win,A_ALTCHARSET);
	    wborder(win,ACS_VLINE,ACS_VLINE,ACS_HLINE,ACS_HLINE,
			ACS_ULCORNER, ACS_URCORNER,ACS_LLCORNER,ACS_LRCORNER);
	//    sw=subwin(win,win->_maxy-2,win->_maxx-2, win->_begy+1,win->_begx+1);
      sw=derwin(win,win->_maxy-1,win->_maxx-1,1,1);
//	    wattroff(win,A_ALTCHARSET);
	    return sw;
    }
    b=(win->_maxx&1)^1;
    wmove(win,0,b);
    waddstr(win,"©°");
    for(i=1;i<=(win->_maxx-4-b-e)/2;i++)waddstr(win,"©¤");
    waddstr(win,"©´");
    for(i=1;i<win->_maxy-1;i++){
    	wmove(win,i,b);
    	waddstr(win,"©¦");
    	wmove(win,i,win->_maxx-2-e);
    	waddstr(win,"©¦");
    }
    wmove(win,i,b);
    waddstr(win,"©¸");
    for(i=1;i<=(win->_maxx-4-b-e)/2;i++)waddstr(win,"©¤");
    waddstr(win,"©¼");
//    wattroff(win,A_ALTCHARSET);
//    sw=subwin(win,win->_maxy-2,win->_maxx-4-b-e,win->_begy+1,win->_begx+2+b);
      sw=derwin(win,win->_maxy-1,win->_maxx-3-b-e,1,2+b);
}
