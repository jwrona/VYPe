#project: VYPe15 programming language compiler
#author: Jan Wrona <xwrona00@stud.fit.vutbr.cz>
#author: Katerina Zmolikova <xzmoli02@stud.fit.vutbr.cz>
#date: 2015

CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -pedantic -O2 -march=native

LEX=flex
LFLAGS=-CFa

YACC=bison
YFLAGS=--defines=parser.h --output=parser.c

PROG=vype
OBJS=parser.o scanner.o hash_table.o data_type.o tac.o builtins.o gen_code.o \
     reg_alloc.o vype.o


all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(OBJS) -o $(PROG)

parser.c: parser.y
	$(YACC) $(YFLAGS) parser.y

#scanner.c: scanner.l

dist:
	tar -czf xzmoli02.tgz scanner.l parser.y hash_table.{c,h} \
		data_type.{c,h} tac.{c,h} builtins.{c,h} gen_code.{c,h} \
		reg_alloc.{c,h} stack.h common.h vype.c \
		Makefile rozdeleni
clean:
	rm -f $(PROG) $(OBJS) parser.c parser.h scanner.c scanner.h
