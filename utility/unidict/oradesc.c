#include "srw.h"
#include <regex.h>

void *malloc(size_t);

static T_PkgType * mkcolumn(FILE *fp,struct srw_s *srwp);
/*
DATABASEAUTHFILE=$(HOME)/etc/rsa.auth
DBLABEL JDSYRSADB
TABNAME tj18d
SKIP 4
COLNUM 33
tjdate 		date(year to day)
unit 		char(8)
tabname		char(8)
flg  		int4
dat1 		int8
dat2 		int8
dat3 		int8
dat4 		int8
dat5 		int8
dat6 		int8
dat7 		int8
dat8 		int8
dat9 		int8
dat10 		int8
dat11 		int8
dat12 		int8
dat13 		int8
dat14 		int8
dat15 		int8
dat16 		int8
dat17 		int8
dat18 		int8
dat19 		int8
dat20 		int8
dat21 		int8
dat22 		int8
dat23 		int8
dat24 		int8
dat25 		int8
dat26 		int8
dat27 		int8
dat28 		int8
dat29 		int8

*/

char YEAR_TO_DAY[]="YYYY.MM.DD";
char YEAR_TO_MIN[]="YYYY.MM.DD NN:MI";
char YEAR_TO_SEC[]="YYYY.MM.DD NN:MI:SS";
static char part[]="^[ 	]*([0-9A-Za-z_]+)[ 	]+([a-z48]{3,8})[ 	]*(\\((.*)\\)){0,1}";
regex_t preg;
#define P_MATCHS 5


T_PkgType mkpkg[]={
	{CH_CHAR,-1,"char",0,-1},
	{CH_DATE,-1,"date"},
	{CH_SHORT,sizeof(short),"short"},
	{CH_INT,sizeof(int),"int"},
	{CH_INT4,sizeof(INT4),"int4"},
	{CH_LONG,sizeof(long),"long"},
	{CH_DOUBLE,sizeof(double),"double"},
	{CH_JUL,sizeof(time_t),"jul",YEAR_TO_DAY},
	{CH_MINUTS,sizeof(time_t),"minuts",YEAR_TO_MIN},
	{CH_CJUL,sizeof(time_t),"cjul","YYYYMMDD"},
	{CH_INT64,sizeof(INT64),"int8"},
	{-1,0}
};

int MkTabDesc(char *flnm,struct srw_s *srwp)
{
FILE *fd;
char buf1[100],buf[1080],*cp;
int cc,i,reclen;

	fd=fopen(flnm,"r");
	if(!fd) {
		perror(flnm);
		return -1;
	}
	srwp->tabname=0;
	while(!ferror(fd)) {
		fgets(buf,sizeof(buf),fd);
		if(feof(fd)) break;
		cp=skipblk(buf);
		TRIM(cp);
		if(*cp=='#') continue;
		if(!strncmp(cp,"DATABASEAUTHFILE",16)) {
			strcfg(cp);
			continue;
		}
		if(!strncmp(cp,"KEYFILE",7)) {
			strcfg(cp);
			continue;
		}
		if((cc=sscanf(cp,"DBLABEL %s",buf1))==1) {
//			if(!srwp->database)
//fprintf(stderr,"DBLABEL=%s\n",buf1);
				strsubst(buf1,0,"DBLABEL=");
				srwp->database=strdup(buf1);
				putenv(srwp->database);
			continue;
		}
		if((cc=sscanf(cp,"TABNAME %s",buf1))==1) {
			if(!srwp->tabname) srwp->tabname=strdup(buf1);
			continue;
		}
		if((cc=sscanf(cp,"SKIP %d",&srwp->skip))==1) {
			continue;
		}
		if((cc=sscanf(cp,"COLNUM %d",&srwp->colnum))==1) {
			srwp->srw_type=mkcolumn(fd,srwp);
			if(!srwp->srw_type) {
				fclose(fd);
				return -1;
			}
			break;
		}
	}
	fclose(fd);
	srwp->colnum=set_offset(srwp->srw_type);
	reclen=srwp->srw_type[srwp->colnum].offset;
	srwp->rec=(char *)malloc(reclen);
	srwp->rem=(char *)malloc(reclen);

	return 0;
}
static T_PkgType *mkcolumn(FILE *fp,struct srw_s *srwp)
{
char buf[1080],*cp,*p,*p1,*p2;
char fldname[41],datatype[9],*format;
regmatch_t pmat[P_MATCHS];
int cc,i,j,k;
T_PkgType *tp;
	if(!srwp->colnum) return 0;
	tp=(T_PkgType *)malloc((srwp->colnum+1) * sizeof(T_PkgType));
	if(!tp) return 0;
	cc=regcomp(&preg,part,REG_EXTENDED);
	if(cc) {
		regerror(cc,&preg,buf,sizeof(buf)-1);
		fprintf(stderr,"regcomp err:%s\n",buf);
		free(tp);
		return 0;
	}
//	set_minuts_default_fmt("%04hd.%02hd.%02hd %02hd:%02hd");
	tp[0].offset=-1;
	j=0;
	while(!ferror(fp)) {
		fgets(buf,sizeof(buf),fp);
		if(feof(fp)) break;
		cp=skipblk(buf);
		if(*cp=='#') continue;
		TRIM(cp);
		if(!*cp) continue;

		if(cc=regexec(&preg,cp,P_MATCHS,pmat,0)) {
			fprintf(stderr,"%s\n",buf);
			regerror(cc,&preg,buf,sizeof(buf));
			fprintf(stderr,"regexec err %d:%s\n",cc,buf);
			continue;
		}
/*
for(k=0;k<P_MATCHS;k++) {
	fprintf(stderr,"pmatch[%d]:%d,%d\n",k,pmat[k].rm_so,pmat[k].rm_eo);
}
*/
		p2=fldname;
		for(p1=cp+pmat[1].rm_so;p1<cp+pmat[1].rm_eo;p1++)
			*p2++=*p1;
                *p2=0;

		p2=datatype;
		for(p1=cp+pmat[2].rm_so;p1<cp+pmat[2].rm_eo;p1++)
			*p2++=*p1;
                *p2=0;

		for(i=0;mkpkg[i].type!=-1;i++) 
			if(!strcmp(datatype,mkpkg[i].name)) break;
		if(mkpkg[i].type==-1) continue;
		tp[j].type=mkpkg[i].type;
		tp[j].name=strdup(fldname);

		p2=fldname;
		if(pmat[4].rm_so>0) {
			for(p1=cp+pmat[4].rm_so;p1<cp+pmat[4].rm_eo;p1++) 
				*p2++=*p1;
		}
		*p2=0;
		if(mkpkg[i].len>0) tp[j].len=mkpkg[i].len;
		else {
			switch(i) {
			case 0:				
				tp[j].len=atoi(fldname)+1;
				break;
			case 1:
				if(!strcmp(fldname,"year to day")) {
					tp[j].len=11;
					tp[j].format=YEAR_TO_DAY;
				} else if(!strcmp(fldname,"year to min")) {
					tp[j].len=17;
					tp[j].format=YEAR_TO_MIN;
				} else if(!strcmp(fldname,"year to sec")) {
					tp[j].len=20;
					tp[j].format=YEAR_TO_SEC;
				} else {
					tp[j].len=11;
					tp[j].format=YEAR_TO_DAY;
				}
				break;
			default:
				tp[j].len=-1;
				break;
			}

		}	
		switch(i) {
		case 6:
			if(*fldname) tp[j].format=strdup(fldname);
			else tp[j].format=0;
			break;
		case 7:
		case 8:
		case 9:
			tp[j].format=mkpkg[i].format;
		case 1:
			break;
		default:
			tp[j].format=0;
			break;
		}
		if(++j < srwp->colnum) continue;
		else break;
	}
	tp[j].type=-1;
	tp[j].len=0;
	regfree(&preg);
	return tp;
}

int print_type(FILE *fd,T_PkgType *tp,char *tabname)
{
char buf[256];
int i;
	fprintf(fd,"struct %s_type[]={\n",tabname);
	i=0;
	while(tp->type != -1) {
		switch(tp->type) {
		case CH_CHAR:
			fprintf(fd,"\t{CH_CHAR,");
			break;
		case CH_DATE:
			fprintf(fd,"\t{CH_DATE,");
			break;
		case CH_INT:
			fprintf(fd,"\t{CH_INT,");
			break;
		case CH_SHORT:
			fprintf(fd,"\t{CH_SHORT,");
			break;
		case CH_LONG:
			fprintf(fd,"\t{CH_LONG,");
			break;
		case CH_DOUBLE:
			fprintf(fd,"\t{CH_DOUBLE,");
			break;
		case CH_JUL:
			fprintf(fd,"\t{CH_JUL,");
			break;
		case CH_CJUL:
			fprintf(fd,"\t{CH_CJUL,");
			break;
		case CH_MINUTS:
			fprintf(fd,"\t{CH_MINUTS,");
			break;
		case CH_INT64:
			fprintf(fd,"\t{CH_INT64,");
			break;
		default:
			tp++;
			continue;
		}
		if(tp->format) sprintf(buf,"\"%s\"",tp->format);
		else sprintf(buf,"0");
		if(!i) fprintf(fd,"%d,\"%s\",%s,-1},\n",tp->len, tp->name,buf);
		else fprintf(fd,"%d,\"%s\",%s},\n",tp->len, tp->name,buf);
		tp++;
		i++;
	}
	fprintf(fd,"\t{-1,0}\n};\n");
	return i;
}
static T_PkgType *freetype(T_PkgType *tp)
{
T_PkgType *tp1;
	if(!tp) return tp;
	for(tp1=tp;tp1->type != -1;tp1++) {
		if(tp1->type==CH_DOUBLE) tp1->format=myfree((char *)tp1->format);
		tp1->name=myfree(tp1->name);
	}
	free(tp);
	return 0;
}
void initsrw(struct srw_s *srwp)
{
	srwp->database=0;
	srwp->tabname=0;
	srwp->srw_type=0;
	srwp->rec=0;
	srwp->rem=0;
	srwp->where=0;
	srwp->select=0;
	srwp->commit=0;
	srwp->uncommit=0;
	srwp->rp=0;
	srwp->R=0;
	srwp->W=0;
	srwp->LOCK=0;
}
void freesrw(struct srw_s *srwp)
{
	srwp->database=myfree(srwp->database);
	srwp->tabname=myfree(srwp->tabname);
	srwp->rem=myfree(srwp->rem);
	srwp->rec=myfree(srwp->rec);
	srwp->where=myfree(srwp->where);
	srwp->select=myfree(srwp->select);
	srwp->srw_type=freetype(srwp->srw_type);
}
char *myfree(char *p)
{
	if(p) {
		free(p);
		p=0;
	}
	return p;
}
