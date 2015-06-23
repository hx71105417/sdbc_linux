/* in zdline.c */
extern double *l;
extern int labn;
putdata(n,argc,argv,x)
int n;
int argc;
char *argv[CALCID];
double x;
{
int i;
double aa;
char val[120];
	if(argc==0 && *argv[0]=='L') {
	   if(n < labn && l ) {
		l[n]=x;
		return(0);
	   }
	   else return(-1);
	}
	else { /*Ìî³äË÷Òý³¡Öµ*/
	   for(i=0;i<argc;i++)
		if(*argv[i+1]) s(srwp,i+1,argv[i+1]);
	}
	aa=(long)round(x);
	sprintf(val,"%lg",aa);
#ifdef DBWR   /* pdb */
	w(srwp,n,val);
#else   /* gdb */
	put_one(srwp->rec,val,srwp->srw_type,srwp->skip+n-1);
#endif
	return(0);
}
