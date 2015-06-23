//连接
static int sc_connect(pool *p1,resource *rs)
{
int ret=-1;
T_NetHead Head;
struct utsname ubuf;
char buf[200],*p,addr[20];
log_stu logs;

	ret=Net_Connect(&rs->Conn,&rs->cli,*p1->log.family?p1->family:NULL);
	if(ret) {
		rs->cli.Errno=errno;
		stptok(strerror(errno),rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		return -1;
	}
//login
	uname(&ubuf);
	p=buf;
	Head.O_NODE=LocalAddr(rs->Conn.Socket,addr);
	p+=sprintf(p,"%s|%s|%s,%s|||",p1->log.DEVID,p1->log.LABEL,
		ubuf.nodename,addr);
	rs->Conn.MTU=p1->log.MTU;
	Head.PROTO_NUM=0;
	Head.D_NODE=p1->log.NEXT_d_node;
	Head.ERRNO1=rs->Conn.MTU;
	Head.ERRNO2=Head.PKG_REC_NUM=0;
	Head.data=buf;
	Head.PKG_LEN=strlen(Head.data);
	ret=SendPack(&rs->Conn,&Head);
	ret=RecvPack(&rs->Conn,&Head);
	if(ret) {
		rs->cli.Errno=errno;
		stptok(strerror(errno),rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		disconnect(&rs->Conn);
		ShowLog(1,"%s:network error %d,%s",__FUNCTION__,rs->cli.Errno,rs->cli.ErrMsg);
		rs->cli.Errno=-1;
		return -2;
	}
	if(Head.ERRNO1 || Head.ERRNO2) {
		ShowLog(1,"%s:login error ERRNO1=%d,ERRNO2=%d,%s",__FUNCTION__,
			Head.ERRNO1,Head.ERRNO2,Head.data);
		disconnect(&rs->Conn);
		stptok(Head.data,rs->cli.ErrMsg,sizeof(rs->cli.ErrMsg),0);
		rs->cli.Errno=-1;
		return -3;
	}
	net_dispack(&logs,Head.data,log_tpl);
	strcpy(rs->cli.DBOWN,logs.DBOWN);
	strcpy(rs->cli.UID,logs.DBUSER);
//取服务名
	ret=init_svc_no(&rs->Conn);
	rs->cli.Errno=ret;
	*rs->cli.ErrMsg=0;
	return ret;
}
