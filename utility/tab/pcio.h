#include <curses.h>
#include <term.h>
#include <ctype.h>
/*   Í¨ÐÅ¿ØÖÆÂë     */

#define NUL 0
#define SOH 1
#define STX 2
#define ETX 3
#define EOT 4
#define ENQ 5
#define ACK 6
#define BEL 7
#define HT_  9
#define LF_  0x0a
#define VT_  0x0b
#define FF_  0x0c
#define CR_  0x0d
#define SO_  0x0e
#define SI_  0x0f
#define DLE 0x10
#define DC1 0x11
#define XON 0x11
#define DC2 0x12
#define DC3 0x13
#define XOFF 0x13
#define DC4 0x14
#define NAK 0x15
#define SYN 0x16
#define ETB 0x17
#define CAN 0x18
#define EM_  0x19
#define SUB 0x1a
#define ESC 0x1b
#define FS_  0x1c
#define GS_  0x1d
#define RS_  0x1e
#define US_  0x1f
#define DEL 0x7f
extern char BS_;

#define Bs		0x08 		/*  BS     */
#define Tab		0x09 		/* Htab    */
#define Return		0x0d		
#define Ctrl_A		0x01
#define Ctrl_B		0x02
#define Ctrl_C		0x03
#define Ctrl_D		0x04
#define Ctrl_E		0x05
#define Ctrl_F		0x06
#define Ctrl_G		0x07
#define Ctrl_H		0x08
#define Ctrl_I		0x09
#define Ctrl_J		0x0a
#define Ctrl_K		0x0b
#define Ctrl_L		0x0c
#define Ctrl_M		0x0d
#define Ctrl_N		0x0e
#define Ctrl_O		0x0f
#define Ctrl_P		0x10
#define Ctrl_Q		0x11
#define Ctrl_R		0x12
#define Ctrl_S		0x13
#define Ctrl_T		0x14
#define Ctrl_U		0x15
#define Ctrl_V		0x16
#define Ctrl_W		0x17
#define Ctrl_X		0x18
#define Ctrl_Y		0x19
#define Ctrl_Z		0x1a
#define Esc		0x1b
#define _FS		0x1c /* Ctrl_\ */
#define _GS		0x1d /* Ctrl_] */
#define Ctrl_6		0x1e
#define Ctrl_NEG	0x1f /* Ctrl_- */

#define _B _ISblank
#define _N _ISdigit

#define accept(y,x,str,n) waccept(stdscr,y,x,str,n);


#ifndef ACCE
extern int accemode;
extern int accestat;
#endif

#define LEFTOUT 256
#define RIGHTOUT 512
#define UPOUT 1024
#define DOWNOUT 2048
#define ENTCLR  0x80000000


//char *wsave(WINDOW *);
WINDOW *wcbox(WINDOW *);
//int wputtext(WINDOW *,char *);
char *strsubst(char * to_be_substituted,int doomed,char *by_str);
char *stptok(char *from,char *to,int sizeof_to,char *break_chars);
//char * regcmp(char *partten,char *,...); //end by (char *)0
// reg_point=regcmp(partten,(char *)0);
//char * regex(char *reg_point,...);
char * skipblk(char *str);

void wdispn(WINDOW *winp,unsigned char *str,int pos,int n);
void wco(WINDOW *winp,unsigned c);

