char *strsubst(from,j,str)
char *from,*str;
int j;
{
int i;
register char *cp,*cp1,*cp2;
 	if(!from) return 0;
	i=strlen(from);
	if(j<0) j=0;
	else if(j>i) j=i;
	else ;
	i=str?strlen(str):0;
	if(i<j) {  /* delete some char*/
		cp1=from+i;
		cp=from+j;
		while(*cp) *cp1++=*cp++;
		*cp1=0;
	}
	else if (i>j) { /* extend some*/
		cp2=from+j;
		cp=from+strlen(from);
		cp1=cp+i-j;
		while(cp >= cp2) *cp1-- = *cp--;
	}
	else ;
	if(str) strncpy(from,str,i);
	return from+i;
}
char * strins(char *str,char c)
{
char p[2];
	p[0]=c;
	p[1]=0;
	return strsubst(str,0,p);
}
/*
char * strdel(char *str)
{
	return(strsubst(str,1,(char *)0));
}
*/
