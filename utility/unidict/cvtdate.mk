CFLAGS=-I../include  
cvtdate: tcvtdate.o
	cc -o cvtdate tcvtdate.o -L$(HOME)/lib -lsdbc
