CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic-errors -std=c11 -Isrc
SRC = src/errors.c src/lexer.c src/ast.c src/class_decl.c src/parser.c src/module_resolver.c src/trait_resolver.c src/scope_analysis.c src/codegen.c src/ir.c src/ir_verify.c src/ir_print.c src/ir_lower.c src/ir_codegen_c.c src/x86_64_target.c src/x86_64_regalloc.c src/x86_64_instr.c src/x86_64_encode.c src/x86_64_elf.c src/x86_64_codegen.c src/x86_64_link.c src/ir_opt.c src/ir_dominance.c src/ir_ssa.c src/ir_loop.c src/ir_ssa_opt.c src/ir_ipa.c src/ir_profile.c
MAIN_SRC = src/main.c
LDFLAGS = -lm

all: cco gcco cco-link

cco: $(SRC) $(MAIN_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) $(MAIN_SRC) -o cco $(LDFLAGS)

gcco: cco
	@cp -f cco gcco

cco-link: cco
	@cp -f cco cco-link

PREFIX ?= $(HOME)/.local

install: cco gcco cco-link
	@mkdir -p $(PREFIX)/bin
	@cp -f cco $(PREFIX)/bin/cco
	@cp -f gcco $(PREFIX)/bin/gcco
	@cp -f cco-link $(PREFIX)/bin/cco-link
	@mkdir -p $(PREFIX)/lib/cco/std
	@cp -rf std/*.cco $(PREFIX)/lib/cco/std/
	@echo "Successfully installed 'cco', 'gcco', 'cco-link', and standard library to $(PREFIX)"

uninstall:
	@rm -f $(PREFIX)/bin/cco $(PREFIX)/bin/gcco $(PREFIX)/bin/cco-link
	@rm -rf $(PREFIX)/lib/cco
	@echo "Removed 'cco', 'gcco', and standard library from $(PREFIX)"

unit_tests: cco
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_lexer.c -o build/test_lexer $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_parser.c -o build/test_parser $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_scope.c -o build/test_scope $(LDFLAGS)
	$(CC) $(CFLAGS) tests/unit/test_map_runtime.c -o build/test_map_runtime $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_ir.c -o build/test_ir $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_x86_64.c -o build/test_x86_64 $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_linker.c -o build/test_linker $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_opt.c -o build/test_opt $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_ssa.c -o build/test_ssa $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_opt_ssa.c -o build/test_opt_ssa $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_opt_ipa.c -o build/test_opt_ipa $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_opt_pgo.c -o build/test_opt_pgo $(LDFLAGS)
	$(CC) $(CFLAGS) $(SRC) tests/unit/test_syntax.c -o build/test_syntax $(LDFLAGS)
	@echo "--- Running Unit Tests under Valgrind ---"
	valgrind --leak-check=full --error-exitcode=1 ./build/test_lexer
	valgrind --leak-check=full --error-exitcode=1 ./build/test_parser
	valgrind --leak-check=full --error-exitcode=1 ./build/test_scope
	valgrind --leak-check=full --error-exitcode=1 ./build/test_map_runtime
	valgrind --leak-check=full --error-exitcode=1 ./build/test_ir
	valgrind --leak-check=full --error-exitcode=1 ./build/test_x86_64
	valgrind --leak-check=full --error-exitcode=1 ./build/test_linker
	valgrind --leak-check=full --error-exitcode=1 ./build/test_opt
	valgrind --leak-check=full --error-exitcode=1 ./build/test_ssa
	valgrind --leak-check=full --error-exitcode=1 ./build/test_opt_ssa
	valgrind --leak-check=full --error-exitcode=1 ./build/test_opt_ipa
	valgrind --leak-check=full --error-exitcode=1 ./build/test_opt_pgo
	valgrind --leak-check=full --error-exitcode=1 ./build/test_syntax

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

