# mid_status 支持有状态连接的交易管理器  
CFLAGS = -Wall -g -I$(SDBCDIR)/include -I../srvcomm
LFLAGS = -L$(SDBCDIR)/lib 
CC=gcc

OBJS = login.o transfer.o 
#mod_sc.o scpool.o 
#	uc_tcp.o set_sp64.o
midsc: midsc.o $(OBJS)
	$(CC) -o midsc midsc.o $(OBJS) $(SDBCDIR)/lib/AIO_tcp.o $(LFLAGS) -lsc -lscbase -ljson -lpthread -lrt

#$(SDBCDIR)/lib/AIO_tcp.o 
#-L/usr/local/lib -ltcmalloc_minimal

install::midsc
	-mv -f $(HOME)/bin/midsc $(HOME)/bin/midsc_old
	mv midsc $(HOME)/bin

t:t.o
	cc -o t t.o
