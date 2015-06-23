#include <stdio.h>
#include <curses.h>
#include <unistd.h>
#include <stdlib.h>
LANG_readch()
{
char ch;	
	ch=*LANG_readstr;
	if(*LANG_readstr&&LANG_readl>0&&*LANG_readstr!='\n'){
		LANG_readl--;
		LANG_readstr++;
		return(ch);
	}
	else {
		if(*LANG_readstr=='\n'){
		LANG_readl--;
			LANG_readstr++;
		}
		return('\n');
	}
}
