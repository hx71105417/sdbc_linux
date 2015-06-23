/* @(#) acce.c Ver 1.9 88/08/13 */
#include <strproc.h>
#include "pcio.h"

#define ACCE 1
//extern char __ctype_b[];
static  sutab();
static int  sudel();
static  suleft();
static int  suright();
static int  suup();
static int  sudown();
static int  suhome();
static int  curins();
static int  getcur();
static int  hfh();
static int  qch();
static  qline();
static int  wdisp1();
static int  curmv();
static  isacce();

int accemode=0;
int accestat;
/* hight BYTE is OUT range flag
   lower BYTE is ctype flag if it==0 allow all type of char
    else '1' bit is allowed. */

#ifndef MIN
#define MIN(a,b) (((a)>(b))?b:a)
#endif

bool hztz=0;
	struct lis {
	     WINDOW *acewin;
	     unsigned scow;
	     unsigned scor;
	     unsigned scow1; /*unchange cow */
	     unsigned scor1; /*unchange cor */
	     char *proloc; /* porinter unmove */
	     char *bufp;
	     unsigned slen; /*may change line lenth */
	     unsigned n1;
	     int insflag;   /* ins flag */
	  };
static unsigned int ccf;

	wstred(swin,str,n)
WINDOW *swin;
unsigned n;
char *str;
{
	return(waccept(swin,-1,-1,str,n));
}

	waccept(awin,cow,cor,buf,len)
WINDOW *awin;
unsigned cow,cor,len;
char *buf;
{
int ff;
struct lis val;
int c,insflag;
unsigned char n1;
	accestat=0;
	if((len <= 1) || (!buf) || !awin 
	  || (cow==awin->_maxy&&cor==awin->_maxx)) {
	  fputc('\007',stdout); /*bell*/
	  fflush(stdout);
	  return(0);
	}
	getyx(awin,c,n1);
	val.insflag=insflag=ccf=0;
	val.proloc=val.bufp=buf;
	val.slen=len;
	val.acewin=awin;
	val.scow=cow;
	val.scor=cor;
	hfh(&val);
	qch(&val);
	wdisp1(&val);
	curmv(&val,c,n1);
	while(1)  {
		getcur(&val);
		wrefresh(val.acewin);
		c=wgetch(awin);
		if(!iscc(c)) ccf=0;
		else if(!ccf) {
			ccf=c&255;
			continue;
		}
		if((c==KEY_IC)||(c==SOH)) {
		   val.insflag=insflag=1;
		   continue;
		}
		if(c==KEY_END || c==KEY_LL) {
		   accestat |= 1;
		  val.insflag=insflag=0;
		  continue;
		}
		if(c==KEY_LEFT) {
		   ff = val.acewin->_curx>1;
		   accestat |= 1;
		   if(suleft(&val)&accemode) return(c);
		   if(iscc(*val.bufp)&&hztz&&ff)
			wgetch(awin);
		   continue;
		}
		if(c==KEY_RIGHT) {
		   accestat |= 1;
		   if(iscc(*val.bufp)&&hztz) wgetch(awin);
		   if(suright(&val)&accemode) return(c);
		   continue;
		}
		if(c==KEY_UP) {
		  val.insflag=insflag=0;
		   if(suup(&val)&accemode) return(c);
		   continue;
		}
		if(c==KEY_DOWN ) {
		  val.insflag=insflag=0;
		  if(sudown(&val)&accemode) return(c);
		  continue;
		}
		if(c==KEY_DC || c==Ctrl_X) {
		   accestat |= 1;
		    val.insflag=insflag=0;
		    c=sudel(&val);
		    continue;
		}
		if(c==Bs)  {
		   accestat |= 1;
		  ff = val.acewin->_curx>1;
		  val.insflag=insflag=0;
		  if(!suleft(&val)) {
		 	if(iscc(*val.bufp)&&hztz&&ff) wgetch(awin);
			sudel(&val);
		  }
		  continue;
		}
		if (c==Ctrl_D) { /*É¾³ýÎ²²¿*/
		   accestat |= 1;
		   val.insflag=insflag=0;
		   *val.bufp='\0';
		   *(val.bufp=val.proloc)=0;
		   qline(&val);
		   continue;
		}
		if(c==KEY_CLEAR) {/*É¾³ýÈ«²¿*/
		   accestat |= 1;
		   *(val.bufp=val.proloc)=0;
		   val.insflag=insflag=0;
		   suhome(&val);
		   qline(&val);
		   continue;
		}
		if(c==KEY_HOME || c==Ctrl_B) {
		   val.insflag=insflag=0;
		   suhome(&val);
		   continue;
		}
		if ((c==Ctrl_R)||(c==Ctrl_L)){
		   wrefresh(curscr);
		   continue;
		}
		if(c==Tab){
			sutab(&val);
			continue;
		}
		if(c&0xff00) return(c);
		n1=c & 255 ;
		if(n1<' ') return(c);
		if(!isacce(n1)) {
			if(n1==' ') return(c);
			wco(awin,'\007');
			continue;
		}
		if(!accestat && !insflag && (accemode&ENTCLR)) {
		   *(val.bufp=val.proloc)=0;
		   suhome(&val);
		   qline(&val);
		}
		accestat=1;
		if(insflag==1) {
		  curins(&val,n1);  /*ins procese*/
		  continue;
		}
		if(val.bufp < (val.proloc+val.n1-(ccf!=0))) {
			c=0;
			if(ccf) {
				if(!*val.bufp)
					val.bufp=strins(val.bufp,ccf);
				else *val.bufp++=ccf;
				wco(val.acewin,ccf&255);
				ccf=0;
			 }
			if(!*val.bufp)
				val.bufp=strins(val.bufp,n1);
			else *val.bufp++=n1;
			val.slen=strlen(val.proloc);
			if(val.acewin->_cury==val.acewin->_maxy-1 &&
			val.acewin->_curx==val.acewin->_maxx-1)
				c=1,val.bufp--;
			wco(val.acewin,n1&255);  /*display a char*/
			if(c) wmove(val.acewin,val.acewin->_maxy-1,
			val.acewin->_maxx-1);
		}
		else {
			wco(val.acewin,'\007'); /*bell*/
		}
	} /*while(1){}*/
}
static  sutab(para)
struct lis *para;
{
int n1,n2;
	n1=(para->acewin->_curx + 8) & ~7;
	n2=n1-para->acewin->_curx;
	if(n1<para->acewin->_maxx-1 && n2<strlen(para->bufp)) {
		para->bufp += n2;
		wmove(para->acewin,para->acewin->_cury,n1);
		getcur(para);
		if(secondcc(para->proloc,para->bufp)) suright(para);
	}
}
static int  sudel(para)
register struct lis *para;
{
int flg=0;
	if(iscc(*para->bufp) && iscc(*(para->bufp+1)))
		strdel(para->bufp),flg=1;
	strdel(para->bufp);
	para->slen=strlen(para->proloc);
	wdisp(para->acewin,para->bufp);
	if(flg) wco(para->acewin,' ');
	wco(para->acewin,' ');
	wmove(para->acewin,para->scow,para->scor);
}
static  suleft(para)
register struct lis *para;
{
	if((para->scow == para->scow1) && (para->scor == para->scor1)) {
	  wmove(para->acewin,para->scow,para->scor);
	  wco(para->acewin,'\007');
	  return(LEFTOUT);
	}
	if(para->scor == 0) {
	  para->scow--;
	  para->scor=para->acewin->_maxx-1;
	}
	para->scor--;
	para->bufp--;
	wmove(para->acewin,para->scow,para->scor);
	if(secondcc(para->proloc,para->bufp)) suleft(para);
	return(0);
}
static int  suright(para)
register struct lis *para;
{
	if((*para->bufp == '\0')||
	  (para->scow==para->acewin->_maxy-1 &&
	  para->scor>=para->acewin->_maxx-2)){
	    wmove(para->acewin,para->scow,para->scor);
	    wco(para->acewin,'\007');
	    return(RIGHTOUT);
	}
	if(para->scor == para->acewin->_maxx-2) {
	  para->scor=0;
	  para->scow++;
	}
	else para->scor++;
	para->bufp++;
	wmove(para->acewin,para->scow,para->scor);
	if(secondcc(para->proloc,para->bufp)&&suright(para))
		suleft(para);
	return(0);
}
static int  suup(para)
register struct lis *para;
{
	if ((para->bufp-para->proloc) <= para->acewin->_maxx-2) {
	  if(!(accemode&UPOUT)) wco(para->acewin,'\007');
	  return(UPOUT);
	}
	para->scow--;
	para->bufp-=para->acewin->_maxx-1;
	if(secondcc(para->proloc,para->bufp)) suright(para);
	wmove(para->acewin,para->scow,para->scor);
	return(0);
}
static int  sudown(para)
register struct lis *para;
{
int n;
register char *p;
	n=strlen(para->proloc);
	p=para->bufp+para->acewin->_maxx-1;
	if(p > para->bufp && p <= para->proloc+n) {
		para->bufp=p;
		para->scow++;
	}
	else {
	  if(!(accemode&DOWNOUT)) wco(para->acewin,'\007');
	  return(DOWNOUT);
	}
	wmove(para->acewin,para->scow,para->scor);
	if(secondcc(para->proloc,para->bufp)) suleft(para);
}
static int  suhome(para)
register struct lis *para;
{
	para->scow=para->scow1;
	para->scor=para->scor1;
	para->bufp=para->proloc;
	wmove(para->acewin,para->scow,para->scor);
}
static int  curins(para,c)
register struct lis *para;
int c;
{
	para->slen=strlen(para->proloc);
	if(para->slen < para->n1-(ccf!=0)) {
		c&=0x00ff;
		getcur(para);  /*rcursor & logcal move*/
		if(ccf) {
			para->bufp=strins(para->bufp,ccf);
			wco(para->acewin,ccf);
			getyx(para->acewin,para->scow,para->scor);
			ccf=0;
		}
		para->bufp=strins(para->bufp,c);
		para->slen=strlen(para->proloc);
		wmove(para->acewin,para->scow1,para->scor1);
		wdisp(para->acewin,para->proloc);
		if (++para->scor >= para->acewin->_maxx-1) {
		    para->scor=0;
		    para->scow++;
		}
		wmove(para->acewin,para->scow,para->scor);
	}
	else wco(para->acewin,'\007');
}
static int  getcur(para)
register struct lis *para;
{
	para->scow=para->acewin->_cury;/*read rcursor*/
	para->scor=para->acewin->_curx;/*read rcursor*/
}
static int  hfh(val)
register struct lis *val;
{
unsigned y,x;
	if(val->scor > (val->acewin->_maxx-2) ||
	   val->scow > (val->acewin->_maxy-1))
		getyx(val->acewin,val->scow,val->scor);
	val->scow1=val->scow;
	val->scor1=val->scor;
	/* screen length*/
	x=(val->acewin->_maxx-1) * val->acewin->_maxy -
	  (val->scow * (val->acewin->_maxx-1) + val->scor)+1;
	val->n1=MIN(val->slen,x)-1;
	val->bufp[val->n1]='\0';
	val->slen=strlen(val->proloc);
}

static int  qch(para)
register struct lis *para;
{
	para->scow=para->scow1;
	para->scor=para->scor1;
	para->bufp=para->proloc;
	qline(para);
	suhome(para);
}
static  qline(para)
register struct lis *para;
{
register n2;
int n,ocol,oline;
	ocol=para->scor,oline=para->scow;
	wmove(para->acewin,para->scow,para->scor);
	n2=(para->scow - para->scow1) * (para->acewin->_maxx-1) +
	   para->scor - para->scor1;
	n2=para->n1 - n2;
	n=para->acewin->_maxx - para->scor;
	if(!n2) return;
	if(n >= n2) {
		wcur(para->acewin,' ',n2);
		return;
	}
	wcur(para->acewin,' ',n);
	n2-=n-1;
	while(n2 > para->acewin->_maxx-1) {
	      para->scow++;
	      para->scor=0;
	      wmove(para->acewin,para->scow,para->scor);
	      wclrtoeol(para->acewin);
	      n2-=para->acewin->_maxx-1;
	}
	para->scow++;
	para->scor=0;
	wmove(para->acewin,para->scow,para->scor);
	if(n2 > 0) wcur(para->acewin,' ',n2);
	para->scor=ocol,para->scow=oline;
	wmove(para->acewin,para->scow,para->scor);
}

static int  wdisp1(para)
register struct lis *para;
{
	wmove(para->acewin,para->scow1,para->scor1);
	wdisp(para->acewin,para->proloc);
	wmove(para->acewin,para->scow,para->scor);
}
static int  curmv(para,y,x)
struct lis *para;
int y,x;
{
register n,i;
int j;
	n=para->acewin->_maxx-1;
	i=n * y + x;
	j=n * para->scow + para->scor;
	if(i<=j) return;
	if(i>=(para->slen+j)) {
		para->bufp+=para->slen;
		i=para->slen+j;
	}
	else {
		para->bufp+=i-j;
	}
	para->scow=i/n;
	para->scor=i%n;
	wmove(para->acewin,para->scow,para->scor);
	if(secondcc(para->proloc,para->bufp)) suleft(para);
}
/*
return 1: allow
return 0: not allow
*/
static  isacce(c)
unsigned char c;
{
int n;
static char digstr[]=".+-@";
	if(!(n=(accemode&255))) return(1);
	if(n&(c>>1)&_B) return(1); /* cc */
	if(c&128) return(0);   /* not allow cc*/
	c&=127;
	if(n==_N && strchr(digstr,c)) return(1);
	if(__isctype(c,n)) return(1);
	else return(0);
}
