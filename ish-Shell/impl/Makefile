CC = gcc
CFLAGS = -g -Wall -Werror

OBJS = ish.o command.o lex.yy.o parser.tab.o

ish: $(OBJS)
	gcc -g -o $@ $(OBJS) -ll

lex.yy.c: lexer.l
	flex lexer.l
parser.tab.c parser.tab.h: parser.y
	bison -d parser.y

lex.yy.o : lex.yy.c parser.tab.h
parser.tab.o : parser.tab.c
command.c: command.h parser.tab.h
ish.c: parser.tab.h

clean:
	rm -f lex.yy.c parser.tab.c parser.tab.h *.o ish y.output
