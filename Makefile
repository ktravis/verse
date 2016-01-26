CFLAGS=-Wall -std=gnu99 -g
OBJS=main.o token.o util.o types.o

compiler: $(OBJS)
	$(CC) $(CFLANGS) -o $@ $(OBJS)

clean:
	rm $(OBJS) 2> /dev/null

run: compiler
	@ cat | ./compiler > tmp.s && gcc tmp.s driver.c -o tmp.out && ./tmp.out
