CC=gcc-5.2
CFLAGS=--std=gnu99 -Wall -Wextra -pedantic -g

LEX=flex
LFLAGS=-CFa

YACC=bison
YFLAGS=--defines=parser.h --output=parser.c

PROG=vype
OBJS=parser.o scanner.o hash_table.o data_type.o vype.o


all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(OBJS) -o $(PROG)

parser.c: parser.y
	$(YACC) $(YFLAGS) parser.y

scanner.c: scanner.l

clean:
	rm -f $(PROG) $(OBJS) parser.c parser.h scanner.c scanner.h
