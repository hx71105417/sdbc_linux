#include $(ORACLE_HOME)/rdbms/lib/env_rdbms.mk
SHELL = /bin/sh
srcdir = .
CC = cc
CPP = cc -E
ECHO = echo
LEX = flex
LFLAGS =  -i
LIBADD_CRYPT = -lcrypt
LIBTOOL = $(SHELL) ../libtool
OBJEXT = o
RANLIB = ranlib
YACC = bison -y
INCLUDES = $(INCLTDL)


LIBS = -lpthread  -Wl,-R$(ORACLE_HOME)/lib -L$(SDBCDIR)/lib -lsqlora8 -L$(ORACLE_HOME)/lib -lsqli -lscry -lstr

#USERLIB=$(ORACLE_HOME)/lib/

LDFLAGS=-L$(LIBHOME) -L$(RDBMSLIB)
LLIBPSO=`cat $(ORACLE_HOME)/rdbms/lib/psoliblist`

# libraries for linking oci programs
OCISHAREDLIBS=$(TTLIBS)
OCISTATICLIBS=$(DEF_OPT) $(NAETTOBJS) $(STATICTTLIBS)

PSOLIBLIST=$(ORACLE_HOME)/rdbms/lib/psoliblist
CLEANPSO=rm -f $(PSOLIBLIST); $(GENPSOLIB)


.SUFFIXES: .o
SHARED_LDFLAG=$(LDFLAGS) -shared -o

CFLAGS=-I$(SDBCDIR)/include  
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(LDFLAGS) -o $@
DIST_COMMON =  ""


.SUFFIXES:
.SUFFIXES: .S .c .lo .o .obj .s

.c.o:
	$(COMPILE) -c $<

.c.obj:
	$(COMPILE) -c `cygpath -w $<`

.s.o:
	$(COMPILE) -c $<

.S.o:
	$(COMPILE) -c $<


.c.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

.s.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

.S.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

SRVOBJ=  hpvar.o srvcomm.o net_login.o 
hpsrv: hpsrv.o gethp.o $(SRVOBJ) $(HOME)/lib/libsdbc.a
	$(CC) $(sql_LDFLAGS) -o hpsrv hpsrv.o gethp.o $(SRVOBJ)  $(LIBS)
#$(OCISTATICLIBS) -lsql9
hpsrv.o:hpsrv.h
hpvar.o:hpdb.c hpdb.h
$(SRVOBJ):../include/sqli.h ../include/netcom.h ../include/netpkg.h hpsrv.h 

cp:
	mv $(HOME)/bin/hpsrv $(HOME)/bin/hpsrv.old 
	mv hpsrv $(HOME)/bin


all: getdb zdsc pdb chkrk midchk 
SRWOBJ=srw.o oradesc.o zdline.o do_line.o cal.o calcu.o getdata.o chkstr.o
# 数据提取程序 
getdb:getdb.o $(SRWOBJ)
	$(CC) $(sql_LDFLAGS) -o getdb getdb.o $(SRWOBJ)  $(LIBS) -lm
#$(OCISTATICLIBS) -lsql8
# 模板生成程序，现在不用了，有mktpl代替之
orasrw:orasrw.o oradesc.o
	cc -o orasrw orasrw.o oradesc.o -L$(SDBCDIR)/lib -lstr
oradesc.o orasrw.o:srw.h

# 字典生成，解决环境变量宏代换和迭代 
zdsc:zdsc.o
	cc -o zdsc zdsc.o -L$(SDBCDIR)/lib -lstr
# 数据存取 
pdb:pdb.o $(SRWOBJ) 
	$(CC) $(sql_LDFLAGS) -o pdb pdb.o $(SRWOBJ)  $(LIBS) -lm 

CHKOBJ=srw.o oradesc.o cal.o calcu.o  chkstr.o 
# 校验入库程序
chkrk:chkrk.o $(SRWOBJ) do_ss.o
	$(CC) $(sql_LDFLAGS) -o chkrk chkrk.o $(CHKOBJ) do_ss.o $(LIBS) -lm
#$(OCISTATICLIBS) -lsql8

$(SRWOBJ):srw.h $(SDBCDIR)/include/sqli.h
getdb.o pdb.o:putdata.c srw.h $(SDBCDIR)/include/sqli.h
calcu.o cal.o getdata.o:calc.h
#表间逻辑校验 
midchk:midchk.o zdline.o $(SRWOBJ)
	$(CC) $(sql_LDFLAGS) -o midchk midchk.o $(CHKOBJ) zdline.o $(LIBS) -lm 
#$(OCISTATICLIBS) -lsql8

