CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic-errors -std=c11 -Isrc -Isrc/common -Isrc/frontend -Isrc/ir -Isrc/backend/c11 -Isrc/backend/x86_64 -Isrc/runtime
SRC = src/common/errors.c src/frontend/lexer.c src/frontend/ast.c src/frontend/class_decl.c src/frontend/parser.c src/frontend/module_resolver.c src/frontend/trait_resolver.c src/frontend/scope_analysis.c src/backend/c11/codegen.c src/ir/ir.c src/ir/ir_verify.c src/ir/ir_print.c src/ir/ir_lower.c src/ir/ir_codegen_c.c src/backend/x86_64/x86_64_target.c src/backend/x86_64/x86_64_regalloc.c src/backend/x86_64/x86_64_instr.c src/backend/x86_64/x86_64_encode.c src/backend/x86_64/x86_64_elf.c src/backend/x86_64/x86_64_codegen.c src/backend/x86_64/x86_64_link.c src/ir/ir_opt.c src/ir/ir_dominance.c src/ir/ir_ssa.c src/ir/ir_loop.c src/ir/ir_ssa_opt.c src/ir/ir_ipa.c src/ir/ir_profile.c
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
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
DESTDIR ?=

INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL) -m 755
INSTALL_DATA ?= $(INSTALL) -m 644

install: cco gcco cco-link
	@mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) cco $(DESTDIR)$(BINDIR)/cco
	$(INSTALL_PROGRAM) gcco $(DESTDIR)$(BINDIR)/gcco
	$(INSTALL_PROGRAM) cco-link $(DESTDIR)$(BINDIR)/cco-link
	@mkdir -p $(DESTDIR)$(LIBDIR)/cco/std
	$(INSTALL_DATA) std/*.cco $(DESTDIR)$(LIBDIR)/cco/std/
	@echo "Successfully installed Cco to $(DESTDIR)$(PREFIX)"

uninstall:
	@if [ -z "$(BINDIR)" ] || [ "$(BINDIR)" = "/" ] || [ -z "$(LIBDIR)" ] || [ "$(LIBDIR)" = "/" ]; then \
		echo "Refusing to uninstall from unsafe paths"; exit 1; \
	fi
	@rm -f $(DESTDIR)$(BINDIR)/cco $(DESTDIR)$(BINDIR)/gcco $(DESTDIR)$(BINDIR)/cco-link
	@rm -rf $(DESTDIR)$(LIBDIR)/cco
	@echo "Removed Cco from $(DESTDIR)$(PREFIX)"

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

test_selfhost_fixpoint: cco
	@mkdir -p build
	@echo "=== Cco Phase 13 Multi-Generation Self-Hosting Bootstrap Fixpoint ==="
	@echo "--- Stage 0 (C compiler): Compiling selfhost/cco.cco -> build/cco_stage1.c ---"
	./cco selfhost/cco.cco -o build/cco_stage1.c
	@echo "--- Compiling Stage 1 executable with strict C11 flags ---"
	$(CC) $(CFLAGS) build/cco_stage1.c -o build/cco_stage1 $(LDFLAGS)
	@echo "--- Stage 1 (Self-hosted): Compiling selfhost/cco.cco -> build/cco_stage2.c ---"
	./build/cco_stage1 selfhost/cco.cco -o build/cco_stage2.c
	@echo "--- Compiling Stage 2 executable with strict C11 flags ---"
	$(CC) $(CFLAGS) build/cco_stage2.c -o build/cco_stage2 $(LDFLAGS)
	@echo "--- Stage 2 (Self-hosted): Compiling selfhost/cco.cco -> build/cco_stage3.c ---"
	./build/cco_stage2 selfhost/cco.cco -o build/cco_stage3.c
	@echo "--- Compiling Stage 3 executable with strict C11 flags ---"
	$(CC) $(CFLAGS) build/cco_stage3.c -o build/cco_stage3 $(LDFLAGS)
	@echo "--- Verifying Bitwise Fixpoint Convergence: Stage 2 vs Stage 3 ---"
	cmp build/cco_stage2.c build/cco_stage3.c
	@diff -u build/cco_stage2.c build/cco_stage3.c
	@echo "--- SHA-256 Checksums: ---"
	@sha256sum build/cco_stage2.c build/cco_stage3.c
	@echo "🎉 Phase 13 Bootstrap Fixpoint Confirmed: Stage 2 == Stage 3 (Bitwise Identical)!"

test_phase13: test_selfhost_fixpoint
	@bash tests/run_phase13_tests.sh

test_phase14: test_bootstrap
	@bash tests/run_phase14_tests.sh

test_phase15: test_bootstrap
	@bash tests/run_phase15_tests.sh

test_phase16: test_bootstrap
	@bash tests/run_phase16_tests.sh

test_phase17: test_bootstrap
	@bash tests/phase17/run_phase17_tests.sh

test_diagnostics: cco
	@bash tests/diagnostics/run_diagnostic_tests.sh

test_phase18b: cco
	@bash tests/test_phase18b_headers.sh

test_phase18c_deque: cco
	@bash tests/test_phase18c_deque.sh

test_phase18c_buffer: cco
	@bash tests/test_phase18c_buffer.sh

test_phase18c_integration: cco
	@bash tests/test_phase18c_integration.sh

test_phase18c: cco
	@bash tests/test_phase18c_option_result.sh
	@bash tests/test_phase18c_deque.sh
	@bash tests/test_phase18c_buffer.sh
	@bash tests/test_phase18c_integration.sh

test_phase18d_io: cco
	@bash tests/test_phase18d_io.sh

test_phase18d_network_io: cco
	@bash tests/test_phase18d_network_io.sh

test_phase18d_tcp_connect: cco
	@bash tests/test_phase18d_tcp_connect.sh

test_phase18d_tcp_lifecycle: cco
	@bash tests/test_phase18d_tcp_lifecycle.sh

test_phase18d: test_phase18d_io test_phase18d_network_io test_phase18d_tcp_connect test_phase18d_tcp_lifecycle

test: unit_tests test_selfhost test_bootstrap test_selfhost_fixpoint test_phase13 test_phase14 test_phase15 test_phase16 test_phase17 test_diagnostics test_phase18b test_phase18c test_phase18d
	@bash tests/run_tests.sh

clean:
	rm -rf build cco gcco selfhost/lexer_selfhosted selfhost/lexer_selfhosted.c selfhost/parser selfhost/parser.c selfhost/typechecker selfhost/typechecker.c selfhost/codegen selfhost/codegen.c selfhost/cco selfhost/cco.c

.PHONY: all install uninstall unit_tests test_selfhost test_bootstrap test_selfhost_fixpoint test_phase13 test_phase14 test_phase15 test_phase16 test_phase17 test_diagnostics test_phase18b test_phase18c test_phase18c_deque test_phase18c_buffer test_phase18c_integration test_phase18d_io test_phase18d_network_io test_phase18d_tcp_connect test_phase18d_tcp_lifecycle test_phase18d test clean

