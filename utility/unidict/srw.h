#define SERVER
#include <sqli.h>

struct srw_s {
	char *database;
	char *tabname;   
	int skip;
	int colnum;
	T_PkgType *srw_type;
	char rowid[20];
	char *rec;        
	char *rem;
	char *select;  /* select %s from xxx a,yyy b */
	char *where;
	char *rp;	
	int R;
	int W;
	int LOCK;
	int commit;
	int uncommit;
	T_SQL_Connect SQL_Connect;
	int curno;
} ;
int MkTabDesc(char *flnm,struct srw_s *srwp);
int print_type(FILE *fd,T_PkgType *tp,char *tabname);
void initsrw(struct srw_s *srwp);
void freesrw(struct srw_s *srwp);
char *myfree(char *p);
void s(struct srw_s *srwp,int num,char *str); 
double readitem(struct srw_s *srwp,int num);
void w(struct srw_s *srwp,int num,char *val);
void rlock(struct srw_s *srwp);
void runlock(struct srw_s *srwp);
int opendb(struct srw_s *srwp);
