CFLAGS = -Wall -O -I$(SDBCDIR)/include 
LFLAGS = -L$(SDBCDIR)/lib 

OBJS = login.o transfer.o mod_sc.o scpool.o
fastmid: fastmid.o $(OBJS)
	cc -o fastmid fastmid.o $(OBJS) $(LFLAGS) -lsc -lscry -lstr -ljson -lpthread

t:t.o
	cc -o t t.o
