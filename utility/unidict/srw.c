#include "srw.h"
#include <stdlib.h>

int readdb(struct srw_s *srwp,int flg);  /* 真读  */
int writedb(struct srw_s *srwp);   /* 真写  */
int pos(struct srw_s *srwp,int num);
int fldlen(struct srw_s *srwp,int num);
char *mkwhere(struct srw_s *srwp,char *buf);
void ptrec(struct srw_s *srwp);
static int Serch(struct srw_s *srwp,int num,char *str);  /* 设  场  */
static void S(struct srw_s *srwp,int num,char *str);  /* 设  场  */
int clrec(struct srw_s *srwp);
char *strchr();

extern time_t today;
extern FILE *logfile;
struct srw_s Table;

void s(srwp,num,str)  /* 调用 S(num,str)  */
struct srw_s *srwp;
int num;
char *str;
{
int cc;

	cc=Serch(srwp,num,str);
	if(!cc) return;
	if(srwp->W) {
		srwp->W=writedb(srwp);
		if(srwp->W) ptrec(srwp);
		if(srwp->commit && ++srwp->uncommit==srwp->commit) {
			___SQL_Transaction__(&(srwp->SQL_Connect),TRANCOMMIT);
			___SQL_Transaction__(&(srwp->SQL_Connect),TRANBEGIN);
			srwp->uncommit=0;
		}
	}
	S(srwp,num,str); srwp->R=-1;
}

static int Serch(srwp,num,str)  /* 设  场  */
struct srw_s *srwp;
int num;
char *str;
{
long nl;
INT64 nll;
int ni;
short ns;
double nd;
float nf;
char datebuf[24];
register char *p,*p1;
	p=srwp->rec+pos(srwp,num-1);
	switch(srwp->srw_type[num-1].type) {
	case CH_CHAR:
		ni=strlen(str);
		if(ni>fldlen(srwp,num-1)) ni=fldlen(srwp,num-1);
		if(!strncmp(p,str,ni)) {
		    if(ni < fldlen(srwp,num-1)-1) {
			ni = *(p+ni);
			if(ni == '\0'||ni==' ') 
				return(0); /* 子集比较问题 */
		    }
	            else return 0;
		}
	        break;
	case CH_DATE:
		nl=cvtdate(str,today);
		rjultostrfmt(datebuf,nl,srwp->srw_type[num-1].format);
		if(!strcmp(p,datebuf)) return 0;
		break;
	case CH_SHORT:
		sscanf(str,"%hd",&ns);
		if(ns==*(short *)p) return 0;
		break;
	case CH_JUL:
	case CH_CJUL:
		ni=cvtdate(str,today);
		if(ni==*(time_t *)p) return 0;
		break;
	case CH_MINUTS:
/*
		while(p1=strchr(str,'.')) {
			*p1++='-';
		}
*/
		ni=rstrmin(str);
		if(nl==*(time_t *)p) return 0;
		break;
	case CH_INT:
		sscanf(str,"%d",&ni);
		if(ni==*(int *)p) return 0;
		break;
	case CH_LONG:
		sscanf(str,"%ld",&nl);
		if(nl==*(long *)p) return 0;
		break;
	case CH_INT64:
		sscanf(str,"%Ld",&nll);
		if(nll==*(INT64 *)p) return 0;
		break;
	case CH_DOUBLE:
		sscanf(str,"%lf",&nd);
		if(nd==*(double *)p) return 0;
		break;
	default:
			return 0;
		break;
	}
	return -1;
}

static void S(srwp,num,str)  /* 设  场  */
struct srw_s *srwp;
register int num;
char *str;
{
time_t tm;
char *p;
char strminuts[23];
	switch(srwp->srw_type[num-1].type) {
	case CH_DATE:
	case CH_JUL:
	case CH_CJUL:
		tm=cvtdate(str,today);
		if(!srwp->srw_type[num-1].format) str=rjulstr(strminuts,tm);
		else str=rjultostrfmt(strminuts,tm,srwp->srw_type[num-1].format);
		break;
/* TEST 
	case CH_MINUTS:  
	tm=rstrmin(str);
	rminstr(strminuts,tm);
	fprintf(logfile,"S MINUTS str=%s,tm=%d,minuts=%s\n",str,tm,strminuts);
		break;
*/
	default:break;
	}
	put_one(srwp->rec,str,srwp->srw_type,num-1,'|');
}
double readitem(srwp,num)  /* 调用 readdb(flg) */
struct srw_s *srwp;
register int num;
{
register char *cp;
char tmp[1024];

	srwp->rp=NULL;
	if(srwp->R==-1) {
		srwp->R=readdb(srwp,srwp->LOCK);
		if(num && srwp->R) {
			ptrec(srwp);
			return 0.;
		}
	}
	if(!num || num>(srwp->colnum-srwp->skip) || num<-srwp->skip) {

		return((double) srwp->R);
	}
	if(num>0)  num--;
	num+=srwp->skip;
get_one(tmp,srwp->rec,srwp->srw_type,num,'|');

	cp=srwp->rec+pos(srwp,num);
 /* 返回所读的数值  */
	if(isnull(cp,srwp->srw_type[num].type)) return DOUBLENULL;
	switch(srwp->srw_type[num].type) {
	case CH_CHAR:
		srwp->rp=cp;
		return((double)strlen(cp));
	case CH_DATE:
		srwp->rp=cp;
		return((double)rstrjul(cp));
	case CH_SHORT:
		return((double)*(short *)cp);
	case CH_INT:
		return((double)*(int *)cp);
	case CH_MINUTS:
		rminstr(srwp->rem,*(time_t *)cp);
		srwp->rp=srwp->rem;

		return((double)*(time_t *)cp);
	case CH_JUL:
	case CH_CJUL:
		rjultostrfmt(srwp->rem,*(time_t *)cp,
				srwp->srw_type[num].format);
		srwp->rp=srwp->rem;
		return((double)*(time_t *)cp);
	case CH_LONG:
		return((double)*(long *)cp);
	 case CH_INT64:
                return((double)*(INT64 *)cp);
	case CH_DOUBLE:
		return(*(double *)cp);
	default:
		return(0.);
	}
}

int readdb(srwp,flg)  /* 真读  */
struct srw_s *srwp;
int flg;
{
int cc,i;
char tmp[4096],*rec=0;
char tmp1[2048];
char statment[4096];
	mkfield(tmp,srwp->srw_type,0), mkwhere(srwp,tmp1);
/*
fprintf(stderr,"select %s from %s where %s\n",tmp,
	srwp->tabname, tmp1);
*/
	if(!*srwp->SQL_Connect.DBOWN)
		sprintf(statment,"select %s from %s where %s",
			 tmp, srwp->tabname, tmp1);
	else
		sprintf(statment,"select %s from %s.%s where %s",
			 tmp, srwp->SQL_Connect.DBOWN,srwp->tabname, tmp1);

	if(flg) strcat(statment,"for update");

	cc=___SQL_Select__(&srwp->SQL_Connect,statment,&rec,1);
	if(cc) {
		ShowLog(1,"srw readdb:%s,err=%d,%s",statment,
			srwp->SQL_Connect.Errno,
			srwp->SQL_Connect.ErrMsg);
			rec=myfree(rec);
		clrec(srwp);
	}
	else net_dispack(srwp->rec,rec,srwp->srw_type);
	rec=myfree(rec);
/*
get_one(tmp,srwp->rec,srwp->srw_type,0,'|');
ShowLog(1,"get_one[0]=%s",tmp);
*/
/*
for(i=0;i<5;i++) {
	get_one(tmp,srwp->rec,srwp->srw_type,i,'|');
	fprintf(stderr,"%s,",tmp);
}
fprintf(stderr,"\n");
*/
	return(cc);  /* -> R */
}

void w(struct srw_s *srwp,int num,char *val)   /* 调用writedb(flg) */
{
	if(!num || num>(srwp->colnum-srwp->skip) || num<-srwp->skip) return;
	if(srwp->R==-1) srwp->R=readdb(srwp,srwp->LOCK);  /* s-r-w lost LOCK */
	if(num>0) num--;
	num += srwp->skip;
	put_one(srwp->rec,val,srwp->srw_type,num,'|');
	srwp->W=-1;
}

int writedb(struct srw_s *srwp)   /* 真写  */
{
int cc;
char tmp[4048],stmt[4048],value[4048],where[1024];
	/* R==0 readdb sucess */
	cc=0;
	if(srwp->R) {
		strcpy(tmp,"writedb");
		cc=insert_db(&srwp->SQL_Connect,srwp->tabname,srwp->rec,
						srwp->srw_type,tmp);
		if(!cc) return 0;
		else if(srwp->SQL_Connect.Errno != DUPKEY) {
			ShowLog(1,"writedb:%s",tmp);
			return cc;
		}
	}
	net_pack(tmp,srwp->rec,srwp->srw_type);
	if(!*srwp->SQL_Connect.DBOWN)
		sprintf(stmt,"update %s %s where %s", srwp->tabname,
			mkupdate(value,tmp,srwp->srw_type),
			mkwhere(srwp,where));
	else
		sprintf(stmt,"update %s.%s %s where %s",
			srwp->SQL_Connect.DBOWN,
			srwp->tabname,
			mkupdate(value,tmp,srwp->srw_type),
			mkwhere(srwp,where));

	cc=___SQL_Exec(&srwp->SQL_Connect,stmt);
	if(cc!=1) {
		ShowLog(1,"srw update err=%d,errmsg=%s,stmt=%s",
			cc,srwp->SQL_Connect.ErrMsg,stmt);
		ptrec(srwp);
		return(-1);
	}
	return(0);
}

void rlock(struct srw_s *srwp)
{
	srwp->LOCK=1;
}
void runlock(struct srw_s *srwp)
{
	srwp->LOCK=0;
}
void ptrec(struct srw_s *srwp)
{
char tmp[256];
register int i,j,n;
	for(i=0;i<srwp->skip;i++)
	{
		get_one(tmp,srwp->rec,srwp->srw_type,i,'|');
		fprintf(stderr,"%s,",tmp);
	}
	if(srwp->R) fprintf(stderr,"  R error = %d\n",srwp->R);
	if(srwp->W>0) fprintf(stderr," W error =%d\n",srwp->W);
}
int pos(srwp,num)
struct srw_s *srwp;
int num;
{
	return(srwp->srw_type[num].offset);
}
fldlen(srwp,num)
struct srw_s *srwp;
int num;
{
	return(srwp->srw_type[num].len);
}
char *mkwhere(struct srw_s *srwp,char *buf)
{
int i;
char *p,value[500],tmp[2048];
	p=buf;
	for(i=0;i<srwp->skip;i++) {
		p+=sprintf(p,"%s %s ",
			!i?"":"and",srwp->srw_type[i].name);
		get_one(tmp,srwp->rec,srwp->srw_type,i,'|');
		switch(srwp->srw_type[i].type) {
		case CH_MINUTS:
		case CH_DATE:
		case CH_JUL:
		
			if(!*tmp) {
				p+=sprintf(p,"is null ");
				break;
			}
			else if(srwp->srw_type[i].format) {
				p+=sprintf(p,"= to_date('%s','%s') ",
					tmp,srwp->srw_type[i].format);
				break;
			}
		case CH_CHAR:
		case CH_CJUL:
			p+=sprintf(p,"= '%s' ",tmp);
			break;
		default:
			if(*tmp) p+=sprintf(p,"= %s ",tmp);
			else p+=sprintf(p,"is null ");
			break;
		}
	}
	return buf;
}

int opendb(struct srw_s *srwp)
{
int ret;
/*
char tmp[256];
	if(!srwp->database) return 1;
ShowLog(5,"srw opendb %s",srwp->database);
	ret=SQL_AUTH(srwp->database,getenv("DBLABEL"), srwp->SQL_Connect.DSN,
			srwp->SQL_Connect.UID, srwp->SQL_Connect.PWD,
			srwp->SQL_Connect.DBOWN);
	if(ret) return POINTERR;
	ret=___SQL_OpenDatabase__( &srwp->SQL_Connect);
*/
	ret=db_open(&srwp->SQL_Connect);
/*
	if(*srwp->SQL_Connect.DBOWN) {
	char *cp=0;
		cp=strsubst(srwp->tabname,0,srwp->SQL_Connect.DBOWN);
		cp=strsubst(cp,0,".");
	}
*/
	return ret;
}
int clrec(struct srw_s *srwp)
{
T_PkgType *tp;
char *cp;
	for(tp=srwp->srw_type+srwp->skip;tp->type != -1;tp++) {
		cp=srwp->rec+tp->offset;
		switch(tp->type&127) {
		case CH_SHORT:
			*(short *)cp=0;
			break;
		case CH_INT:
			*(int *)cp=0;
			break;
		case CH_LONG:
			*(long *)cp=0;
			break;
		case CH_DOUBLE:
			*(double *)cp=0;
			break;
		case CH_INT64:
			*(INT64 *)cp=0;
			break;
		default:
			*cp=0;	
			break;
		
		}
	}
}
