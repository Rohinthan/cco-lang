CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic-errors -std=c11 -Isrc
SRC = src/errors.c src/lexer.c src/ast.c src/class_decl.c src/parser.c src/module_resolver.c src/scope_analysis.c src/codegen.c
MAIN_SRC = src/main.c
LDFLAGS = -lm

all: cco

cco: $(SRC) $(MAIN_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) $(MAIN_SRC) -o cco $(LDFLAGS)

unit_tests: cco
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_lexer.c -o build/test_lexer $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_parser.c -o build/test_parser $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_scope.c -o build/test_scope $(LDFLAGS)
	$(CC) $(CFLAGS) tests/unit/test_map_runtime.c -o build/test_map_runtime $(LDFLAGS)
	@echo "--- Running Unit Tests under Valgrind ---"
	valgrind --leak-check=full --error-exitcode=1 ./build/test_lexer
	valgrind --leak-check=full --error-exitcode=1 ./build/test_parser
	valgrind --leak-check=full --error-exitcode=1 ./build/test_scope
	valgrind --leak-check=full --error-exitcode=1 ./build/test_map_runtime

test_selfhost: cco
	@bash tests/compare_lexers.sh

test: unit_tests test_selfhost
	@bash tests/run_tests.sh

clean:
	rm -rf build cco selfhost/lexer_selfhosted selfhost/lexer_selfhosted.c target.cco

.PHONY: all unit_tests test_selfhost test clean
