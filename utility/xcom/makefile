CFLAGS= -m64 -Wall -I$(SDBCDIR)/include -O
LFLAG=-L$(SDBCDIR)/lib
CC=gcc

all:xcomd xcom filels 
#简单的通信服务器
#当含有family_old.o模块时，xcomd具有与xcom不同的内码，因此不能互相通信，必须通过midware转发 发 
xcomd:xcomd.o f_srv.o event_s.o 
	$(CC) -s -o xcomd xcomd.o event_s.o f_srv.o $(LFLAG) -lsc -lscbase -lpthread -lrt

xcomd.o getlist.o f_srv.o : xcomd.h $(SDBCDIR)/include/sc.h $(SDBCDIR)/include/scsrv.h $(SDBCDIR)/include/strproc.h

#简单的通信客户端
xcom:xcom.o clilogin.o getls.o putls.o f_cli.o event_c.o 
	cc -s -o xcom xcom.o clilogin.o getls.o putls.o f_cli.o event_c.o $(LFLAG) -lsc -lscbase -lpthread
xcom.o cli_login.o sendfile.o getls.o putls.o f_cli.o event_c.o:xcom.h $(SDBCDIR)/include/sccli.h $(SDBCDIR)/include/sc.h $(SDBCDIR)/include/strproc.h

#网络安全隔离中间件 
#midware:midware.o transfer.o 
#	cc -o midware midware.o transfer.o $(LFLAG) -lsc  -lpthread
#midware.o transfer.o : $(SDBCDIR)/include/scsrv.h $(SDBCDIR)/include/sccli.h $(SDBCDIR)/include/sc.h

# 文件列表实用程序 
filels:filels.o 
	$(CC) -m64 -s -o filels filels.o $(LFLAG) -lscbase
#测试程序 
t:t.o
	cc -o t t.o $(LFLAG) -lscbase
