CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic-errors -std=c11 -Isrc
SRC = src/errors.c src/lexer.c src/ast.c src/class_decl.c src/parser.c src/module_resolver.c src/trait_resolver.c src/scope_analysis.c src/codegen.c
MAIN_SRC = src/main.c
LDFLAGS = -lm

all: cco gcco

cco: $(SRC) $(MAIN_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) $(MAIN_SRC) -o cco $(LDFLAGS)

gcco: cco
	@cp -f cco gcco

PREFIX ?= $(HOME)/.local

install: cco gcco
	@mkdir -p $(PREFIX)/bin
	@cp -f cco $(PREFIX)/bin/cco
	@cp -f gcco $(PREFIX)/bin/gcco
	@mkdir -p $(PREFIX)/lib/cco/std
	@cp -rf std/*.cco $(PREFIX)/lib/cco/std/
	@echo "Successfully installed 'cco', 'gcco', and standard library to $(PREFIX)"

uninstall:
	@rm -f $(PREFIX)/bin/cco $(PREFIX)/bin/gcco
	@rm -rf $(PREFIX)/lib/cco
	@echo "Removed 'cco', 'gcco', and standard library from $(PREFIX)"

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

test_bootstrap: cco
	@mkdir -p build
	@cp -f selfhost/target.cco target.cco
	@echo "--- Transpiling Self-Hosted Modules ---"
	./cco selfhost/parser.cco -o selfhost/parser.c
	$(CC) $(CFLAGS) selfhost/parser.c -o selfhost/parser $(LDFLAGS)
	./cco selfhost/typechecker.cco -o selfhost/typechecker.c
	$(CC) $(CFLAGS) selfhost/typechecker.c -o selfhost/typechecker $(LDFLAGS)
	./cco selfhost/codegen.cco -o selfhost/codegen.c
	$(CC) $(CFLAGS) selfhost/codegen.c -o selfhost/codegen $(LDFLAGS)
	./cco selfhost/cco.cco -o selfhost/cco.c
	$(CC) $(CFLAGS) selfhost/cco.c -o selfhost/cco $(LDFLAGS)
	@echo "--- Running Self-Hosted Pipeline under Valgrind ---"
	valgrind --leak-check=full --error-exitcode=1 ./selfhost/parser
	valgrind --leak-check=full --error-exitcode=1 ./selfhost/typechecker
	valgrind --leak-check=full --error-exitcode=1 ./selfhost/codegen target.cco build/bootstrap_target.c
	valgrind --leak-check=full --error-exitcode=1 ./selfhost/cco target.cco -o build/bootstrap_target.c
	@echo "--- Building Emitted C with Strict Flags ---"
	$(CC) $(CFLAGS) build/bootstrap_target.c -o build/bootstrap_target $(LDFLAGS)
	@echo "--- Verifying Bootstrap Executable under Valgrind ---"
	valgrind --leak-check=full --error-exitcode=1 ./build/bootstrap_target > build/bootstrap_actual.txt
	@./cco target.cco --run > build/bootstrap_expected.txt
	@diff -u build/bootstrap_actual.txt build/bootstrap_expected.txt
	@echo "Bootstrap parity test: PASSED (100% match, 0 leaks, 0 errors)"

test: unit_tests test_selfhost test_bootstrap
	@bash tests/run_tests.sh

clean:
	rm -rf build cco gcco selfhost/lexer_selfhosted selfhost/lexer_selfhosted.c selfhost/parser selfhost/parser.c selfhost/typechecker selfhost/typechecker.c selfhost/codegen selfhost/codegen.c selfhost/cco selfhost/cco.c

.PHONY: all install uninstall unit_tests test_selfhost test_bootstrap test clean

