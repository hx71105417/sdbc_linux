/* @(#) 1.2   91/04/17 */
#define ADD  0
#define SUBT 1
#define MUL  4
#define DIV  5
#define MOD  6

#define CALCID 9

double calc(double,int,double);
double calculator(char *,double (*)(int,int,char *[CALCID]));
int let(char *,double,int (*)(int,int,char *[CALCID],double));
int strlet(char *,char *,int (*)(int,int,char *[CALCID],char *));
double fround(double x,int flg,int dig);
