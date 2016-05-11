CFLAGS=-Wall -std=gnu99 -g
OBJS=main.o src/token.o src/util.o src/types.o src/parse.o src/ast.o src/var.o src/eval.o

compiler: prelude $(OBJS)
	$(CC) $(CFLAGS) -o bin/$@ $(OBJS)

clean:
	@rm -rf bin prelude.bin prelude.h $(OBJS) 2>/dev/null

test: compiler
	for f in tests/*.vs; do ./verse $$f; done 

prelude: prelude.c
	mkdir -p bin
	$(CC) binpack.c -o bin/includer
	bin/includer $^
