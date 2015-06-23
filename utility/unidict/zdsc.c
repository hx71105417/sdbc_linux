/*   zdsc: for LINUX 
 usage: zdsc zd >outputdict
 zd:     for file @1
	    x,@1,y,,n;
	 endfor

	 for - @1  #stdin
	   x,@1,y,,n;
	 endfor
	 stdin: Ctrl_L for next group;

	 for file @1,@2
	            ^--------------------
		x,@1,@2,y,n;            |
	 endfor                         |-----same,exclude @[0-9]
	 file:   for@1,for@2            |
	              ^------------------
	 for <! @1
	 a
	 b
	 c
	 !
	 ..
	 ..
	 endfor
Subtitute by envenroment :
		$(ENVname)
*/
#include <stdio.h>
#include <ctype.h>
#include <regex.h>
#include <sdbc.h>
#include <string.h>


char *dir,*dirname(char *);
char term_src[]="^@[0-9]+";
regex_t term_rp;
void addrp();
void freerp();
void freezd();
static char merr[]="zdsc malloc err!\n";
int dictsc();
static char *__loc1;
#define MAXREG 5

main(argc,argv)
int argc;
char *argv[];
{
FILE *fd;
char *cp,*p1,*p2;
int cc;
	if(argc<2) {
		fprintf(stderr,"usage: %s dict [ENV=val ...]\n",argv[0]);
		exit(1);
	}
	if(argc>2) {
		for(cc=2;cc<argc;cc++) {
			putenv(argv[cc]);
		}
	}
	cc=regcomp(&term_rp,term_src,REG_EXTENDED);
	if(cc) {
	char errbuf[256];
		regerror(cc,&term_rp,errbuf,sizeof(errbuf));
	    	fprintf(stderr,"comile %s err %d",term_src,errbuf);
		exit(1);
	}
	if(!(fd=fopen(argv[1],"r"))) {
/*
	    	free(term_rp);
		perror(argv[1]);
		exit(1);
*/
		fd=stdin;
		dir=dirname(argv[1]);
		if(!dir) dir=getenv("DICTDIR");
	}
	else dir=dirname(argv[1]);
	if(!dir) dir=".";
	cc=dictsc(fd,0);
	fclose(fd);
	regfree(&term_rp);
	return cc;
}

struct zdser {
	regex_t rp;
	char *rep;
	char sepc;
	struct zdser *next;
};

struct zdstru {
	FILE *fp;
	char *tmpfile;
	long pos;
	int n;
	struct zdser *zdp;
};

	
int dictsc(fd,zdsp)
FILE *fd;
struct zdstru *zdsp;
{
char line[1024];
struct zdstru zdop;
char *cp,*dp,*p,aa[256],bb[512],endc;
regmatch_t pmatch[MAXREG];
int cc;
	while(fgets(line,sizeof(line),fd)) {
		cc=strcfg(line);
		if(1==sscanf(line,"PUTENV %s",aa)) {
			putenv(strdup(aa));
			continue;
		}
		cp=skipblk(line);
		if(!strncmp(cp,"endfor",6)) {
			if(zdsp) {
				if(cc=getfzd(zdsp)) return cc;
				fseek(fd,zdsp->pos,0);
			}
			continue;
		} else ;
		if(*(dp=stptok(cp,aa,sizeof(aa)," 	"))) {
			TRIM(aa);
			if(!strcmp(aa,"for")) {
				zdop.n=0;
				zdop.zdp=0;
				zdop.tmpfile=0;
				zdop.fp=stdin;
				zdop.pos=ftell(fd);
				cp=dp++;
				sscanf(cp,"%s",aa);
				if(*aa=='<') {
				    endc=aa[1];
				    strcpy(bb,"/usr/tmp/ZDSCXXXXXX");
				    cc=mkstemp(bb);
				    zdop.tmpfile=strdup(bb);
				    if(!zdop.tmpfile) return(2);
				    zdop.fp=fdopen(cc,"w+rb");
				    if(!zdop.fp) {
					perror(zdop.tmpfile);
					free(zdop.tmpfile);
					return(2);
				    }
                                    while(fgets(aa,sizeof(aa),fd)) {
					p=skipblk(aa);
					if(*p==endc) {
					    zdop.pos=ftell(fd);
					    fseek(zdop.fp,0L,0);
					    break;
					}
					fputs(aa,zdop.fp);
				    }
				} else if(*aa != '-') {
					sprintf(bb,"%s/%s",dir,aa);
					if(!(zdop.fp=fopen(bb,"r"))) {
						perror(bb);
errret:
						if(zdop.tmpfile) {
						    fclose(zdop.fp);
						    unlink(zdop.tmpfile);
						    free(zdop.tmpfile);
						}else if(zdop.fp!=stdin)
					                fclose(zdop.fp);
						return(2);
					}
				}
				else ;

				if(!(dp=strchr(cp,'@'))) {
					fprintf(stderr,"%s miss @\n",
					       bb);
					goto errret;
				}
/* for filename @1|@2|@3|@4|  
           dp-> ^^^^^^^^^^^^
*/
				while(!regexec(&term_rp,dp,MAXREG,pmatch,0)) {
				   __loc1=dp;
				   dp += pmatch[0].rm_eo;
				   cc=pmatch[0].rm_eo-pmatch[0].rm_so;
				   strncpy(aa,__loc1,cc);
				   aa[cc]=0;
				   if(cc=mkrp(&zdop,aa,dp)) {
					fprintf(stderr,
					    "mkrp:malloc err\n");
				        goto errret;
				   }
				   if(!*dp) break;
				   dp++;
				} 
				if(zdsp) addrp(zdop.zdp,zdsp->zdp);
				if(cc=getfzd(&zdop)) {
					if(cc==-1) return(cc);
					fprintf(stderr,"fzd err!\n");
				        goto errret;
				}
				cc=dictsc(fd,&zdop);
				freezd(&zdop);

				continue;
			}
		}
		if(zdsp) subst(zdsp,line);
		fputs(line,stdout);
	}
	return 0;
}
mkrp(zp,cp,dp)
struct zdstru *zp;
char *cp,*dp;
{
regex_t rp;
struct zdser *zsp,*zs1;
int ret;
	ret=regcomp(&rp,cp,REG_EXTENDED);
	if(ret) return 1;
	zsp=(struct zdser *)malloc(sizeof(struct zdser));
	if(!zsp) {
		regfree(&rp);
		return 1;
	}
	zsp->rp=rp;
	zsp->rep=0;
	zsp->next=0;
	zsp->sepc=*dp;
	zs1=zp->zdp;
	zp->n++;
	if(!zs1) zp->zdp=zsp;
	else {
	    addrp(zs1,zsp);
	}
	return 0;
}
void addrp(zs1,zsp)
struct zdser *zs1,*zsp;
{
    if(!zsp) return;
    while(zs1->next) zs1=zs1->next;
    zs1->next=zsp;
}
void freezd(zp)
struct zdstru *zp;
{
int i;
struct zdser *rp;
	if(!(i=zp->n)) return;
	rp=zp->zdp;
	while(--i) rp=rp->next;
	rp->next=0;
	freerp(zp->zdp);
	zp->zdp=0;
	zp->n=0;
	if(zp->fp != stdin) fclose(zp->fp);
	if(zp->tmpfile) {
	    unlink(zp->tmpfile);
	    free(zp->tmpfile);
	}
	return;
}

void freerp(rp)
struct zdser *rp;
{
	if(!rp) return;
	if(rp->next) freerp(rp->next);
	regfree(&rp->rp);
	if(rp->rep) free(rp->rep);
	free(rp);
}

/* È¡¸¨×Öµä*/
getfzd(zdp)
struct zdstru *zdp;
{
char aa[512],bb[256],*cp,tok[20];
int cc,i;
struct zdser *zp;
    do {
	if(!(fgets(aa,sizeof(aa),zdp->fp))) {
		return -1;
	}
	if((zdp->fp==stdin) && (*aa=='\f')) {
		return -2;
	}
    } while(*(cp=skipblk(aa))=='#'||!*cp);


	zp=zdp->zdp;
	for(i=0;i<zdp->n && *cp;i++,zp=zp->next) {
	    if(zp->sepc) sprintf(tok,"%c\r\n",zp->sepc);
	    else strcpy(tok,"\r\n");

	    cp=stptok(cp,bb,sizeof(bb),tok);
	    zp->rep=strdup(bb);
	    if(!zp->rep) {
		    fprintf(stderr,merr);
		    return(1);
	    }
	    if(*cp) cp++;
	}
	return 0;
}
subst(zdp,line)
struct zdstru *zdp;
char *line;
{
register char *cp,*cp1,*cp2;
struct zdser *zp;
int i,j,k;
regmatch_t pmatch[MAXREG];
	for(zp=zdp->zdp;zp;zp=zp->next) {
	     cp=line;
	     while(!regexec(&zp->rp,cp,MAXREG,pmatch,0)) {
		__loc1= cp+pmatch[0].rm_so;
		cp += pmatch[0].rm_eo;
		if(isdigit(*cp)) continue;
		if(*cp=='$' || *cp=='\\') cp++;
		cp=strsubst(__loc1,cp-__loc1,zp->rep);
	     }
	}
}
