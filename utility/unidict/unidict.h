#include <fdbc.h>

/*
T_PkgType mkpkg_s[]={
	{CH_CHAR,-1,"char",0,-1},
	{CH_DATE,-1,"date"},
	{CH_SHORT,sizeof(short),"short"},
	{CH_INT,sizeof(int),"int"},
	{CH_INT4,sizeof(INT4),"int4"},
	{CH_LONG,sizeof(long),"long"},
	{CH_DOUBLE,sizeof(double),"double"},
	{CH_JUL,sizeof(long),"jul"},
	{-1,0}
};
*/

#define TEAR_TO_DAY 11
#define TEAR_TO_MIN 17
#define TEAR_TO_SEC 20
struct srw_s {
	char *tabname;     // dbown.tabname
	int colnum;
	T_PkgType *srw_type;
	char rowid[20];
	char *rec;        // record data
	char *bak;
	char *select;  /* select %s from xxx a,yyy b */
	char *where;
	int flg;
	SQL_Connect *sqlp;
	int curno;
};
/*

TABNAME tj18d
START 4
COLNUM 10
18date 		date(year to sec)
18unit 		char(8)
18name 		char(8)
18flg  		short
18dat1 		long
18dat2 		long
18dat3 		long
18dat4 		long
18dat5 		long
18dat6 		double(3.1lf)

*/
