.PHONY=compiler
CFLAGS=-Wall -std=gnu99 -g -Werror -Wno-error=unused-variable
OBJS=main.o src/ast.o src/codegen.o src/eval.o src/parse.o src/scope.o src/semantics.o src/token.o src/types.o src/util.o src/var.o

compiler: bin/compiler

bin/compiler: prelude.bin $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

main.o: main.c prelude.bin
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf bin prelude.bin prelude.h $(OBJS) 2>/dev/null

test: compiler
	@for f in tests/*.vs; do echo "Testing $$f..."; ./verse $$f >/dev/null; done;

bin/includer: binpack.c
	mkdir -p bin
	$(CC) $^ -o $@

prelude.bin: prelude.c bin/includer
	mkdir -p bin
	bin/includer $<
