CFLAGS=-Wall -std=gnu99 -g
OBJS=main.o src/token.o src/util.o src/types.o src/parse.o

compiler: $(OBJS)
	$(CC) $(CFLANGS) -o $@ $(OBJS)

clean:
	rm $(OBJS) 2> /dev/null

test: compiler
	@cat x.vs | ./compiler > tmp.c && gcc -o tmp.out tmp.c && ./tmp.out
