
LIBDIR=$(SDBCDIR)/lib
INCDIR=$(SDBCDIR)/include
CFLAGS= -m64 -Wall -fPIC -I$(INCDIR) -g
CC=cc
all:aio $(LIBDIR)/libsc.a

aio:$(LIBDIR)/AIO_tcp.o $(LIBDIR)/AIO_fd.o
$(LIBDIR)/AIO_tcp.o:AIO_tcp.c
	$(CC) $(CFLAGS) -c -o $@ $<

	
LIBOBJS= chkexec.o echo.o filelist.o rexec.o  PutEvent.o jerr.o \
	get_srvname.o PPC_srv.o TPC_srv.o clikey.o set_showid.o Function.o \
		 tpool.o SIO_fd.o mod_sc.o 
$(LIBDIR)/libsc.a:$(LIBOBJS)
	ar -rv $(LIBDIR)/libsc.a $?
$(LIBOBJS):$(INCDIR)/scsrv.h $(INCDIR)/sc.h
#linux 2.6.22 ртио
$(LIBDIR)/AIO_fd.o:AIO_fd.c
	$(CC) $(CFLAGS) -c -o $@ $<

