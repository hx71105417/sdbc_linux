#include <stdio.h>
#include <ctype.h>
#include <sdbc.h>

int nullmod=0;
#include "calc.h"
double calc(x,n,y)
double x,y;
int n;
{
	if(nullmod) {
		if(isnull(&x,CH_DOUBLE) ||
			isnull(&y,CH_DOUBLE)) return(DOUBLENULL);
	} else {
		if(isnull(&y,CH_DOUBLE)) {
			if(n==MOD) return(0.);
			else return(x);
		}
	}
	switch(n) {
	case ADD:return(x+y);
	case SUBT:return(x-y);
	case MUL:return(x*y);
	case DIV:
		if(y) return(x/y);
		else return(0.);
	case MOD:
		if((long)y) return((double)((long)x%(long)y));
	default:return(0.);
	}
}
