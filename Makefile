CFLAGS=-Wall -std=gnu99 -g
OBJS=main.o token.o util.o types.o parse.o

compiler: $(OBJS)
	$(CC) $(CFLANGS) -o $@ $(OBJS)

clean:
	rm $(OBJS) 2> /dev/null
