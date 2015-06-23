#include <sdbc.h>

struct where_s { // used by mkwhere()
	int num;
	char relation[10];
	char link[12];
};
char *mkwhere(T_PkgType *tp,void *data,int i,char *relation,char buf[]);

char *mkwhere(T_PkgType *tp,void *data,struct where_s *wp,char buf[])
{
char *p,tmp[2048];
int i;
	p=buf;
	p+=sprintf(p,"%s ", tp[i].name);
	for(i=0;*wp[i].link;i++) {
		if(wp[i].num < 0) continue;
		get_one(tmp,data,tp,wp[i].num);
		switch(tp[wp[i].num].type) {
		case CH_MINUTS:
		case CH_DATE:
		case CH_JUL:
			if(!*tmp) {
				if(*relation=='=') p+=sprintf(p,"is null");
				else p+=sprintf(p,"is not null");
				break;
			}
			else if(srwp->srw_type[i].format) {
				p+=sprintf(p,"%s to_date('%s','%s') ",
					relation,tmp,tp[i].format);
				break;
			}
		case CH_CHAR:
			p+=sprintf(p,"%s '%s' ",relation,tmp);
			break;
		default:
			if(*tmp) p+=sprintf(p,"%s %s ",relation,tmp);
			else if(*relation=='=') p+=sprintf(p,"is null");
			else p+=sprintf(p,"is not null");
			break;
		}
	}
	return buf;
}
