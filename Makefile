CFLAGS=-Wall -std=gnu99 -g
OBJS=main.o src/token.o src/util.o src/types.o src/parse.o src/ast.o src/var.o src/eval.o

compiler: $(OBJS)
	$(CC) $(CFLANGS) -o $@ $(OBJS)

clean:
	rm $(OBJS) 2> /dev/null

test: compiler
	for f in tests/*.vs; do ./verse $$f; done 
