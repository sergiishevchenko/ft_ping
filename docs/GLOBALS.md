# Global variables — inventory and justification

The 42 **Norm** does not set a numeric limit on globals. It requires:

- every global name to start with `g_`;
- every global use to be **justifiable**.

The **ft_ping** subject does not add further restrictions. This document records each file-scope variable in the project, why it exists, and what alternatives were rejected.

Related: [ARCHITECTURE.md](ARCHITECTURE.md), [concepts/SIGNALS.md](concepts/SIGNALS.md), [GETOPT-LONG.md](GETOPT-LONG.md), [FLAGS.md](FLAGS.md) (`-r`).

---

## Summary

| Name | File | Linkage | Justified? |
|------|------|---------|------------|
| `g_stop` | `srcs/main.c` | external (`extern` in `ft_ping.h`) | **Yes** — signal handler cannot receive `t_ping *` |
| `g_dontroute` | `srcs/main.c` | external (`extern` in `srcs/socket.c`) | **Yes** — parsed before socket exists; read in `socket.c` |
| `g_long_opts` | `srcs/main.c` | `static` (file-local) | **Yes** — constant table required by `getopt_long` |

**Count:** 2 external globals, 1 `static` file-scope table. All other session state lives in `t_ping ping` on the stack in `main()`.

---

## `g_stop`

**Type:** `volatile sig_atomic_t`  
**Defined:** `srcs/main.c`  
**Declared:** `includes/ft_ping.h` (`extern`)  
**Written by:** `srcs/signal.c` (`sig_int_handler`)  
**Read by:** `srcs/main.c` (`ping_loop`)

### Why it is needed

`SIGINT` (Ctrl+C) must stop the ping loop without killing the process immediately. After the loop ends, `main()` prints statistics and exits cleanly — matching inetutils behaviour.

The POSIX signal handler signature is fixed:

```c
void handler(int sig);
```

It receives only the signal number. There is no parameter for user context, so the handler cannot access `t_ping *ping` unless that pointer is stored somewhere the handler can reach.

### Why not put it in `t_ping`?

`ping` is a local variable in `main()`. Passing its address into `sigaction` would require either:

- a global pointer to `t_ping` (still a global, and worse: async-signal-unsafe if the handler touched `ping` fields), or
- `SA_SIGINFO` and a more complex setup — still needs a global or static slot for the pointer.

A single integer flag is the minimal, standard pattern for “please stop the loop”.

### Why `volatile sig_atomic_t`

| Qualifier | Reason |
|-----------|--------|
| `sig_atomic_t` | POSIX guarantees read/write of this type is atomic inside a signal handler |
| `volatile` | The compiler must not cache `g_stop` in a register; `while (!g_stop)` must re-read it every iteration |

The handler only sets `g_stop = 1`. It does not call `printf`, `free`, or any other async-signal-unsafe function. Statistics run in normal code after the loop.

### Alternatives rejected

| Alternative | Why rejected |
|-------------|--------------|
| `exit()` inside the handler | Skips statistics; wrong UX |
| Longjmp from the handler | Fragile; easy to corrupt state |
| Read `ping` fields from the handler | Async-signal-unsafe |
| Poll-only loop without signals | Ctrl+C would not stop the program promptly |

---

## `g_dontroute`

**Type:** `int`  
**Defined:** `srcs/main.c`  
**Declared:** `srcs/socket.c` (`extern int g_dontroute`)  
**Written by:** `srcs/main.c` (`parse_args`, when `opt == 'r'`)  
**Read by:** `srcs/socket.c` (`set_sock_options`, `SO_DONTROUTE`)

### Why it is needed

Flag `-r` enables `SO_DONTROUTE` on the raw socket. The option is parsed in `parse_args()` but applied in `set_sock_options()` inside `socket.c`.

Call order in `main()`:

```
init_ping(&ping)
parse_args(&ping, ...)     ← -r may appear here
create_socket(&ping)       ← set_sock_options() runs here
```

At parse time the socket does not exist yet. The `-r` decision must survive until `create_socket()`.

### Why not only `t_ping`?

It could be stored as `ping->dontroute` (or a bit in `ping->options`). That would remove the external global and is a valid refactor.

The current design keeps `-r` outside `t_ping` because:

- it is a **socket creation** flag, not runtime ping state (TTL, interval, stats);
- `socket.c` already isolates all `setsockopt` logic; reading one `extern int` avoids threading a flag only used at socket setup through `t_ping`;
- inetutils-style code often treats `SO_DONTROUTE` as a socket-layer switch separate from the ping session struct.

Both approaches are defensible at evaluation. The global is justified as a small bridge between argument parsing and socket setup across two modules.

### Alternatives rejected

| Alternative | Why rejected (for now) |
|-------------|------------------------|
| Parse `-r` after `create_socket` | Breaks normal CLI order; socket would be created without `SO_DONTROUTE` first |
| Call `setsockopt` from `parse_args` | Socket does not exist during parsing |
| Duplicate `-r` state in two places | Risk of inconsistency |

**Possible improvement:** move to `ping->options` or `ping->dontroute` and drop `g_dontroute` if stricter Norm compliance is desired.

---

## `g_long_opts`

**Type:** `static struct option[]`  
**File:** `srcs/main.c` only  
**Used by:** `getopt_long()` in `parse_args()`

### Why it is needed

`getopt_long` requires a `struct option` array describing long options (`--ttl`, `--ip-timestamp`, `--help`). This table is:

- constant for the lifetime of the program;
- only referenced from `parse_args()` in the same file.

It is declared `static` so it does not pollute the global namespace or other translation units.

### Why this is not a “bad” global

File-scope `static` data is technically global storage duration, but:

- it has **internal linkage** — invisible outside `main.c`;
- it is read-only configuration, not mutable session state;
- the Norm allows declaration and initialisation on the same line for globals and static variables.

Naming with `g_` follows the Norm prefix rule for global storage.

### Alternatives rejected

| Alternative | Why rejected |
|-------------|--------------|
| Build the table on the stack each call | Wasteful; table is fixed |
| Put the table in `ft_ping.h` | Violates Norm (no struct definitions in `.c` only is fine; exposing implementation detail in header is unnecessary) |
| Hard-code only short options | Subject requires `--ttl` and `--ip-timestamp` |

---

## Libc globals (not project globals)

`getopt_long` uses globals from `<unistd.h>` / `<getopt.h>`. The project reads them but does not define them:

| Name | Used in | Role |
|------|---------|------|
| `optarg` | `handle_option()`, `parse_number()` | argument string for the current option |
| `optind` | `parse_args()` | index of next `argv` element after options |
| `optopt` | `parse_args()` | invalid option character for error messages |
| `opterr` | `parse_args()` | set to `0` to suppress libc error spam |

These are part of the POSIX `getopt` API, not project design choices. See [OPTARG.md](OPTARG.md) and [GETOPT-LONG.md](GETOPT-LONG.md).

---

## Evaluation checklist

When asked “why globals?” on defense:

1. **Norm** — names use `g_`; each variable has a documented reason on this page.
2. **State model** — almost everything is in `t_ping`; globals are exceptions, not the default.
3. **`g_stop`** — unavoidable without a more complex signal setup; handler stays minimal and async-signal-safe.
4. **`g_dontroute`** — bridges CLI parsing and socket setup before `ping->sockfd` exists; could move into `t_ping` if requested.
5. **`g_long_opts`** — static constant table for `getopt_long`; not shared mutable state.
