#include "srw.h"

/* 模板生成程序，现在不用了，有mktpl代替之 

TABNAME tj18d
SKIP 4
COLNUM 10
18date 		date(year to day)
18unit 		char(8)
18name 		char(8)
18flg  		short
18dat1 		long
18dat2 		long
18dat3 		long
18dat4 		long
18dat5 		long
18dat6 		double(%3.1lf)

*/
struct srw_s Table;
int main(int argc,char *argv[])
{
int cc;
	if(argc < 2) {
		fprintf(stderr,"Usage:%s descfile\n",argv[0]);
		return 1;
	}
	initsrw(&Table);
	cc=MkTabDesc(argv[1],&Table);
	fprintf(stderr,"Tabname=%s:skip=%d:colnum=%d,cc=%d\n",
		Table.tabname,Table.skip,Table.colnum,cc);
	print_type(stdout,Table.srw_type,Table.tabname);
	freesrw(&Table);
	return 0;
}

