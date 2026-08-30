# Cco Networking Architecture & Hardening Audit

This document records the baseline audit, edge-case vulnerability analysis, hardened runtime implementations, portability constraints, and concurrency model for Cco's native POSIX networking subsystem (`net_listen`, `net_accept`, `net_recv`, `net_send`, `net_close`, `sleep_ms`).

---

## 1. Initial State Baseline Audit

Before this hardening pass, Cco included basic POSIX socket functions in `src/stdlib_prelude.h`. The initial audit revealed the following implementation details and edge-case vulnerabilities:

### Function-by-Function Baseline Audit

| Function | Cco Signature | Underlying Syscall | Initial Behavior | Identified Vulnerabilities |
|---|---|---|---|---|
| `net_listen` | `net_listen(port: int) -> int` | `socket()`, `setsockopt()`, `bind()`, `listen()` | Created `AF_INET` socket on `INADDR_ANY:port` with `SO_REUSEADDR` and backlog 128. | Correctly cleaned up `server_fd` on bind/listen failure. |
| `net_accept` | `net_accept(server_fd: int) -> int` | `accept()` | Blocked and accepted incoming connection, returning `client_fd` or `-1`. | Valid baseline. Missing negative fd guard check. |
| `net_recv` | `net_recv(client_fd: int, max_bytes: int) -> string` | `recv()` | Allocated `malloc(max_len + 1)` and made **a single `recv()` syscall**. | **VULNERABILITY: Single-read truncation.** TCP is a stream protocol; a single `recv()` call does not guarantee that the complete HTTP header block or payload has arrived. Multi-packet headers or pipelined requests were silently truncated. |
| `net_send` | `net_send(client_fd: int, data: string) -> int` | `send()` | Looped `while (total < len)`, but passed `flags = 0` and aborted immediately on any `sent <= 0`. | **VULNERABILITY: Process crash on `SIGPIPE`.** If a remote client disconnected mid-transfer, a subsequent `send()` without `MSG_NOSIGNAL` triggers the OS `SIGPIPE` signal, crashing the entire Cco runtime. Furthermore, `EINTR` signals caused premature write aborts. |
| `net_close` | `net_close(fd: int) -> void` | `close()` | Invoked `close(fd)` if `fd >= 0`. | Valid baseline. |
| `sleep_ms` | `sleep_ms(ms: int) -> void` | `nanosleep()` | Paused the calling thread for `ms` milliseconds using POSIX `nanosleep()`. | Valid baseline. |

---

## 2. Hardened Implementation Details

### A. Robust Streaming Loop & HTTP Framing in `net_recv`
TCP delivers an unstructured byte stream. To ensure reliable communication:
1. **Dynamic Buffer Allocation & Initial Capacity Hint**: If `max_bytes <= 0`, an initial buffer (4096 bytes) is allocated. If an explicit size is provided (e.g. `net_recv(client_fd, 512)` or `net_recv(client_fd, 4096)`), it is treated as an **initial buffer capacity hint**; `net_recv` will **automatically and dynamically expand past this initial size** via `realloc` (up to a 1MB safety limit) to accommodate the complete HTTP header and body payload. Passing a small explicit size will never silently truncate incoming requests.
2. **Framing Detection (`\r\n\r\n`)**: `net_recv` accumulates wire packets in a loop until the standard HTTP header delimiter `\r\n\r\n` is detected.
3. **Content-Length Tracking**: If a `Content-Length: <N>` header is present, `net_recv` continues reading until the header plus the entire declared body payload is received (or until EOF / socket error).
4. **Signal & Error Resilience**: Non-fatal signals (`EINTR`) are automatically retried without dropping bytes.
5. **Immediate Disconnect Handling**: If the client connects and disconnects without sending bytes (`recv() == 0`), `net_recv` returns an empty string `""` so caller code immediately bypasses route dispatch and cleanly closes `client_fd`.

### B. Signal-Safe Resilient Writes in `net_send`
1. **`MSG_NOSIGNAL` Flag**: Writes use `MSG_NOSIGNAL` on POSIX platforms to prevent broken-pipe client disconnects from killing the server process.
2. **Full Buffer Loop**: Continues resuming transmission from the unwritten offset `data + total` until all `len` bytes are accepted by the socket buffer.
3. **`EINTR` Recovery**: Automatically handles transient OS interrupts during write operations.

### C. Resource Leak Prevention (Memory & File Descriptors)
1. Every socket allocated via `net_listen()` and `net_accept()` is tracked and closed via `net_close()`.
2. Error branches in route handling (e.g. malformed requests, oversized payloads, unrecognized routes) are guaranteed to execute `net_close(client_fd)` prior to returning.
3. Verified under Valgrind with `--track-fds=yes --leak-check=full` to guarantee **0 byte memory leaks and 0 open file descriptor leaks**.

---

## 3. Concurrency Model & Design Limitations

- **Sequential Connection Handling**: The Cco network server operates **sequentially in a single thread**:
  1. `net_accept()` accepts connection $N$.
  2. `net_recv()` reads request $N$.
  3. Route logic processes request $N$.
  4. `net_send()` writes response $N$.
  5. `net_close()` closes socket $N$.
  6. Server loops back to accept connection $N+1$.
- **Known Limitation**: A slow or hanging client connection will block subsequent clients until the active transaction completes or closes.
- **Future Roadmap**: Asynchronous I/O multiplexing (`epoll`/`kqueue`/event loop) and worker thread pools will be introduced in a future concurrency module.

---

## 4. Portability Statement

Matching the precedent established for POSIX `read_line()` (Chapter 65, v14):
- Cco's networking subsystem directly binds to **POSIX socket headers** (`<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<unistd.h>`).
- Native networking is fully supported on **Linux, macOS, FreeBSD, and Windows Subsystem for Linux (WSL)**.
- Native Windows (MSVC / MinGW without POSIX socket emulation) requires Winsock2 (`ws2_32.lib`, `WSAStartup`), which is documented as unsupported for direct POSIX socket compilation.
