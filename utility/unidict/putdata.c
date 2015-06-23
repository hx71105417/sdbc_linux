/* in zdline.c */
extern double *l;
extern int labn;
putdata(n,argc,argv,str)
int n;
int argc;
char *argv[CALCID];
char *str;
{
int i;
double aa;
char val[120];
	aa=0.;
	if(argc==0 && *argv[0]=='L') {
	   if(n < labn && l ) {
		sscanf(str,"%lf",&l[n]);
		return(0);
	   }
	   else return(-1);
	}
	else { /*Ìî³äË÷Òý³¡Öµ*/
	   for(i=0;i<argc;i++)
		if(*argv[i+1]) s(srwp,i+1,argv[i+1]);
	}
#ifdef DBWR   /* pdb */
	w(srwp,n,str);
#else   /* gdb */
	put_one(srwp->rec,str,srwp->srw_type,srwp->skip+n-1,'|');
#endif
	return(0);
}
