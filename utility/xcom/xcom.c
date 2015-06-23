#include <libgen.h>
#include "xcom.h"

extern int clilogin(T_Connect *conn,T_NetHead *NetHead);
extern int errno;
void e_output(char *str);
void phex(char *title,char *str,int i);

static char myshowid[160];
int main(argc,argv)
int argc;
char *argv[];
{
char *cp;
T_Connect conn;
T_NetHead NetHead;
T_CLI_Var clidata;
int cc,n;
char tmp[1024];
char *p;
char values[512];
char dir[512];

	if(argc < 2) {
		fprintf(stderr,"Usage: %s cfgfile [lgGeE]\n",argv[0]);
		return 1;
	}
	envcfg(argv[1]);
	strcpy(myshowid,basename(argv[0]));
	Showid=myshowid;
	Init_CLI_Var(&clidata);

	cp=getenv("HOST");
	if(!cp||!*cp) cp="localhost";
	strcpy(conn.Host,cp);
	cp=getenv("SERVER");
	if(!cp||!*cp) cp="xcom";
	strcpy(conn.Service,cp);
//printf("host=%s,server=%s\n",conn.Host,conn.Service);
	conn.timeout=0;
	cc=Net_Connect(&conn,&clidata,0);
//printf("after Net_Connect cc=%d\n",cc);
	if(cc<0) {
		ShowLog(1,"Connect %s/%s cc=%d errno=%d,%s\n",
			conn.Host,conn.Service,cc,errno,strerror(errno));
		freeconnect(&conn);
		return 2;
	}
	cc=clilogin(&conn,&NetHead);
	ShowLog(2,"aft login cc=%d,ERRNO1=%d,ERRNO2=%d,data=%s\n",cc,
		NetHead.ERRNO1,NetHead.ERRNO2,NetHead.data);
	if(cc||NetHead.ERRNO2) {
		StrAddr(NetHead.O_NODE,tmp);
		if(NetHead.PKG_LEN) {
	//		printf("Login err %d :%s,e_node=%s\n",
	//			cc,NetHead.data,tmp);
		} else {
			ShowLog(1,"Login cc=%d errno2=%d,e_node=%s",
					cc,NetHead.ERRNO2,tmp);
		}
		disconnect(&conn);
		return 5;
	}
	strcpy(dir,NetHead.data);
	conn.Event_proc=event_catch;
	if((n=NetHead.PKG_LEN) !=0) {
		ShowLog(3,"Login success %s\n",NetHead.data);
	}

//取服务名 
	cc=init_svc_no(&conn);
	if(cc) {
		ShowLog(1,"init_svc_no Error!");
	}
	if(n) {
ShowLog(5,"ChDir to:%s",dir);
		if(0!=(cc=N_ChDir(&conn,dir))) {
                        ShowLog(1,"Can Not change dir to %s err=%d",
                                dir,cc);
                        disconnect(&conn);
                        return 6;
                }
	}
	else  ShowLog(3,"Login success \n");
/*
printf("peeraddress=%s:\n",peeraddr(conn.Socket,buf));
cc=LocalAddr(conn.Socket,values);
printf("LocalAddr=%s,%08X\n",values,cc);
*/
cc=LocalAddr(conn.Socket,values);
//printf("LocalAddr=%s,cc=%08X\n",values,cc);
//phex("cc=",(char *)&cc,4);

	if(argc==2 || *argv[2]=='l')
		while(!ferror(stdin)) {
			fscanf(stdin,"%80s",tmp);
			if(feof(stdin)) break;
			cc=getls(&conn,&NetHead,tmp,stdout);
			if(cc<0) break;
		}
	else if(*argv[2]=='g') cc=s_getfile(&conn,stdin);
	else if(*argv[2]=='G') {
		while(!ferror(stdin)) {
		char *p;
			fgets(tmp,sizeof(tmp),stdin);
			if(feof(stdin)) break;
			p=skipblk(tmp);
			TRIM(p);
			if(!*p) continue;
//now=now_usec();
			cc=m_getfile(&conn,&NetHead,p);
//printf("get %s at %lldus\n",p,now_usec() - now);
			if(cc) ShowLog(1,"aft m_getfile ret=%d",cc);
			if(cc<0) break;
		}
	} else if (*argv[2]=='P') {
		while(!ferror(stdin)) {
			fgets(tmp,sizeof(tmp),stdin);
			if(feof(stdin)) break;
			p=skipblk(tmp);
			TRIM(p);
ShowLog(5,"Putfile:%s",tmp);
			putlist(&conn,tmp);
		}
	} else if (*argv[2]=='e' || *argv[2]=='E') {
		while(!ferror(stdin)) {
			fgets(tmp,sizeof(tmp),stdin);
			if(feof(stdin)) break;
			p=skipblk(tmp);
			if(*p=='#') continue;
			TRIM(p);
			if(*argv[2]=='e') cc=N_Rexec(&conn,tmp,0,0);
			else if(*argv[2]=='E') {
				cp=stptok(skipblk(tmp),values,4,"	 ");
				if(!strcmp(values,"cd")) {
					cp=skipblk(cp);
					cc=N_ChDir(&conn,cp);
					ShowLog(1,"xcom chdir %s %d\n",cp,cc);
						
				} else {
					cc=N_Rexec(&conn,tmp,0,e_output);
					ShowLog(5,"xcom rexec %s return %d",tmp,cc);
					if(cc<0) break;
				}
			}
		}
	} else {
		ShowLog(1,"%s unknow",argv[2]);
	}
	
	disconnect(&conn);
	ShowLog(3,"%s END!",argv[0]);
	return 0;
}
void e_output(char *str)
{
	printf("%s",str);
}
void FreeVar(char *p)
{
}
void phex(char *title,char *str,int i)
{
int j;
char errbuf[400];
char *p;
	strcpy(errbuf,title);
	strcat(errbuf," ");
	p=errbuf+strlen(errbuf);
	while(i>64) {
		i-=64;
		for(j=0;j<64;j++) p+=sprintf(p,"%02X ",*str++ & 255);
		fprintf(stderr,"%s\n",errbuf);
		p=errbuf;
	}
	for(j=0;j<i;j++) p+=sprintf(p,"%02X ",*str++ & 255);
	printf("%s\n",errbuf);
}
