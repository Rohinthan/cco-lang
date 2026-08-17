CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -Isrc
SRC = src/lexer.c src/ast.c src/class_decl.c src/parser.c src/scope_analysis.c src/codegen.c
MAIN_SRC = src/main.c

all: cco

cco: $(SRC) $(MAIN_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) $(MAIN_SRC) -o cco

unit_tests: cco
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_lexer.c -o build/test_lexer
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_parser.c -o build/test_parser
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_scope.c -o build/test_scope
	@echo "--- Running Unit Tests under Valgrind ---"
	valgrind --leak-check=full --error-exitcode=1 ./build/test_lexer
	valgrind --leak-check=full --error-exitcode=1 ./build/test_parser
	valgrind --leak-check=full --error-exitcode=1 ./build/test_scope

test: unit_tests
	@bash tests/run_tests.sh

clean:
	rm -rf build cco

.PHONY: all unit_tests test clean
