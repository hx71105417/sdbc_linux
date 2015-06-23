CFLAGS= -O -I$(SDBCDIR)/include
LFLAG= -L$(SDBCDIR)/lib
TOOLOBJ= wco.o wdispn.o wcur.o wcbox.o acce.o wdisp.o
tcur:tcur.o $(TOOLOBJ)
	cc -o tcur tcur.o $(TOOLOBJ) $(LDLAG) -lpanel -lcurses -lstr
tpad:tpad.o
	cc -o tpad tpad.o -lpanel -lcurses
$(TOOLOBJ): pcio.h
# 屏幕浏览程序
btab:btab.o $(TOOLOBJ)
	cc -o btab btab.o $(TOOLOBJ) $(LFLAG) -lstr -lncurses
# 通用制表程序
ctab:ctab.o setattr.o
	cc -o ctab ctab.o setattr.o $(LFLAG) -lstr 
