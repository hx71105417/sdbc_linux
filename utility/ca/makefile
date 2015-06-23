
CFLAGS = -w -m64 -g -I$(SDBCDIR)/include

all: mkpass mklabel mkdevid

mkpass:mkpass.o 
	cc -m64 -o mkpass mkpass.o -L$(SDBCDIR)/lib -lscbase
mklabel:mklabel.o 
	cc -m64 -o mklabel mklabel.o  -L$(SDBCDIR)/lib -lscbase
mkdevid:mkdevid.o
	cc -m64 -o mkdevid mkdevid.o -L$(SDBCDIR)/lib -lscbase

