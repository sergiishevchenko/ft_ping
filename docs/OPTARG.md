# `optarg` and numeric argument parsing

When `handle_option()` runs this line:

```c
ping->count = (size_t)parse_number(optarg, LONG_MAX, "count");
```

`optarg` does not come from `ping`, `main`, or a function parameter. It is a **global variable** filled by **`getopt_long`** one call earlier in `parse_args()`.

This page traces the full path **`argv` → `getopt_long` → `optarg` → `parse_number` → `t_ping`**, documents every related global from `<getopt.h>`, and explains how `parse_number()` validates the string.

For long options, `struct option`, and the option loop, see [GETOPT-LONG.md](GETOPT-LONG.md). For flag meanings, see [FLAGS.md](FLAGS.md).

---

## The big picture

```bash
sudo ./ft_ping -c 3 --ttl 64 8.8.8.8
```

```
shell
  │
  ▼
main(argc, argv)          argv[3] = "3", argv[5] = "64", …
  │
  ▼
parse_args()
  │
  ├─ getopt_long()  ──► sets optarg, optind, returns option code
  │
  ├─ handle_option(ping, opt)
  │     └─ parse_number(optarg, …)   // reads global optarg
  │
  └─ resolve_host(argv[optind])      // hostname after all flags
```

| Stage | Who | What moves |
|-------|-----|------------|
| 1 | shell | builds `argv[]` |
| 2 | `getopt_long` | picks next flag; if it needs a value, sets **`optarg`** |
| 3 | `handle_option` | uses **`opt`** (which flag) + **`optarg`** (its string value) |
| 4 | `parse_number` | converts string → `long`, checks range |
| 5 | caller | casts to `size_t` / `int` / `unsigned long` into `t_ping` |

---

## Where `optarg` is declared

`optarg` is **not** defined anywhere in `ft_ping`. It is declared in system headers:

```c
#include <getopt.h>   /* ft_ping.h line 14 */
```

Typical declaration:

```c
extern char *optarg;
```

It is a **single global pointer** shared by the whole process. Only one option is “current” at a time.

---

## The four `getopt` globals

`getopt_long` is **stateful**: it remembers progress through `argv` in globals instead of hiding everything in return values.

| Global | Type | Set by | Meaning in `ft_ping` |
|--------|------|--------|----------------------|
| **`optarg`** | `char *` | `getopt_long` | Argument string for the **current** option (`"3"`, `"64"`, …) |
| **`optind`** | `int` | `getopt_long` | Index in `argv` of the **next** token to examine |
| **`optopt`** | `int` | `getopt_long` | Bad option character when return is `'?'` |
| **`opterr`** | `int` | **`parse_args`** | If non-zero, libc prints errors; `ft_ping` sets `opterr = 0` |

### `optarg` — the argument string

Points into memory that originally came from **`argv`** (or from `=` form like `--ttl=64`).

Examples:

| Command fragment | Return of `getopt_long` | `optarg` points to |
|------------------|-------------------------|---------------------|
| `-c 3` | `'c'` | `"3"` |
| `--ttl 64` | `OPT_TTL` (256) | `"64"` |
| `--ttl=64` | `OPT_TTL` | `"64"` |
| `-v` | `'v'` | **undefined** — do not read `optarg` (no argument) |
| `-p deadbeef` | `'p'` | `"deadbeef"` → `decode_pattern`, not `parse_number` |

### `optind` — cursor in `argv`

Starts at `1` (skips `argv[0]` program name). Advances as options are consumed.

After all flags are parsed for:

```text
./ft_ping -v -c 3 --ttl 64 8.8.8.8
```

`optind` points at `argv[6]` (`"8.8.8.8"`). That is why the host loop uses `argv[optind]`, not `optarg`.

### `optopt` — which option failed

Used in the `'?'` branch:

```c
if (optopt && optopt != '?')
    fprintf(stderr, "ft_ping: invalid option -- '%c'\n", optopt);
```

### `opterr` — silence libc

```c
opterr = 0;   /* parse_args — custom messages are printed ft_ping: messages */
```

---

## Step-by-step: one run through the loop

Command:

```bash
sudo ./ft_ping -c 3 8.8.8.8
```

`argv`:

| Index | Value |
|-------|-------|
| 0 | `./ft_ping` |
| 1 | `-c` |
| 2 | `3` |
| 3 | `8.8.8.8` |

### Iteration 1 — `-c 3`

```c
opt = getopt_long(argc, argv, "c:fl:np:rs:T:vw:W:?", g_long_opts, NULL);
```

| After call | Value |
|------------|-------|
| `opt` | `'c'` |
| `optarg` | `"3"` (same bytes as `argv[2]`) |
| `optind` | `3` (next token is host) |

```c
handle_option(ping, 'c');
```

Inside:

```c
ping->count = (size_t)parse_number(optarg, LONG_MAX, "count");
```

Here `optarg` is still `"3"` because **`handle_option` is called before the next `getopt_long`**.

### Iteration 2 — end of options

```c
opt = getopt_long(...);
```

| After call | Value |
|------------|-------|
| `opt` | `-1` |
| loop | exits |

### Host pass

```c
while (optind < argc)   /* optind == 3 */
    resolve_host(ping, argv[3]);   /* "8.8.8.8" */
```

---

## Why `optarg` is not passed into `handle_option`

Current design:

```c
handle_option(ping, opt);          /* only the option code */
parse_number(optarg, ...);         /* optarg read inside */
```

`getopt` has used this pattern since the 1970s: **`opt` in the return value, argument in `optarg`**. Alternatives exist:

| Design | Pros | Cons |
|--------|------|------|
| Global `optarg` (POSIX style) | short call sites; matches `man 3 getopt` | hidden dependency; requires discipline |
| `handle_option(ping, opt, optarg)` | explicit | more parameters on every branch |

`ft_ping` follows the standard libc convention.

---

## Lifetime rules for `optarg`

**Rule:** treat `optarg` as valid only until the **next** `getopt_long` call.

| Safe | Unsafe |
|------|--------|
| use `optarg` immediately in `handle_option` | `char *saved = optarg;` then another `getopt_long` |
| pass `optarg` straight into `parse_number` | store `optarg` in `t_ping` for later |
| `parse_number` reads and exits or returns | async callback using old `optarg` |

`parse_number` only reads the string synchronously — fine.

To retain the text beyond the next `getopt_long` call, copy it:

```c
strdup(optarg);   /* not used in ft_ping */
```

---

## From `optarg` to `parse_number`

### Call sites in `handle_option` (`srcs/main.c`)

| Option | `optarg` example | `parse_number` call | Stored in |
|--------|------------------|---------------------|-----------|
| `-c` | `"10"` | `parse_number(optarg, LONG_MAX, "count")` | `ping->count` (`size_t`) |
| `-l` | `"5"` | `parse_number(optarg, INT_MAX, "preload")` | `ping->preload` (`unsigned long`) |
| `-s` | `"128"` | `parse_number(optarg, PING_MAX_DATALEN, "size")` | `ping->data_length` (`size_t`) |
| `-T` | `"16"` | `parse_number(optarg, 255, "TOS")` | `ping->tos` (`int`) |
| `-w` | `"30"` | `parse_number(optarg, INT_MAX, "timeout")` | `ping->timeout` (`int`) |
| `-W` | `"5"` | `parse_number(optarg, INT_MAX, "linger")` | `ping->linger` (`int`) |
| `--ttl` | `"32"` | `parse_number(optarg, 255, "TTL")` | `ping->ttl` (`int`) |

Non-numeric options use `optarg` differently:

| Option | Function | Notes |
|--------|----------|-------|
| `-p` | `decode_pattern(optarg, …)` | hex string, not decimal |
| `--ip-timestamp` | `strcasecmp(optarg, "tsonly")` | keyword, not number |

### Function signature

```c
long parse_number(const char *str, long max_val, const char *name);
```

| Parameter | At call site | Why this type |
|-----------|--------------|---------------|
| `str` | `optarg` | text from CLI; `const` = read-only |
| `max_val` | `255`, `LONG_MAX`, … | per-flag upper bound; matches `strtol`’s `long` |
| `name` | `"count"`, `"TTL"`, … | label in error messages only |

Return type is `long` because the parser is `strtol`. The caller casts to the field type in `t_ping`.

---

## Inside `parse_number` (`srcs/utils.c`)

```c
long parse_number(const char *str, long max_val, const char *name)
{
    char    *endptr;
    long    val;

    errno = 0;
    val = strtol(str, &endptr, 10);
    if (*endptr != '\0' || errno == ERANGE)
    {
        fprintf(stderr, "ft_ping: invalid %s value: '%s'\n", name, str);
        exit(EXIT_FAILURE);
    }
    if (val < 0 || val > max_val)
    {
        fprintf(stderr, "ft_ping: %s value out of range: '%s'\n", name, str);
        exit(EXIT_FAILURE);
    }
    return (val);
}
```

### Step 1 — `errno = 0`

`strtol` signals overflow via `errno == ERANGE`. Clear `errno` first so an old error is not mistaken for this parse.

### Step 2 — `strtol(str, &endptr, 10)`

| Argument | Role |
|----------|------|
| `str` | input string (same memory as `optarg` when called from `handle_option`) |
| `&endptr` | **output**: where parsing stopped |
| `10` | base 10 (decimal) |

### Step 3 — “entire string must be a number”

```c
if (*endptr != '\0' || errno == ERANGE)
```

| Failure | Example |
|---------|---------|
| trailing junk | `"3abc"`, `"12.5"` → `endptr` not at `'\0'` |
| overflow | huge string → `ERANGE` |

### Step 4 — range check

```c
if (val < 0 || val > max_val)
```

Rejects `-1`, `--ttl 300`, etc., even if `strtol` succeeded.

### Step 5 — return

Caller assigns with a cast, e.g. `(size_t)` or `(int)`.

---

## Pointers: `str` vs `&endptr`

Two different pointer roles in one function:

```
INPUT (read string):
    const char *str  ──►  '3' '\0'     ← same as optarg for "-c 3"

OUTPUT (write position):
    char *endptr;                    local variable
    strtol(str, &endptr, 10);        &endptr = address of endptr
                                     strtol stores pointer into endptr

CHECK:
    *endptr != '\0'   →  characters left after the number?
```

| Expression | Type | Points to |
|------------|------|-----------|
| `str` | `const char *` | first character of argument string |
| `endptr` | `char *` | first **unparsed** character (set by `strtol`) |
| `&endptr` | `char **` | the variable `endptr` itself (so `strtol` can write into it) |

`str` is **input** (data to read). `&endptr` is **output** (where to store a result pointer). Both are “pointers,” but the direction differs.

---

## Why `const` on `str` but not on `max_val`

| Parameter | Passing | `const` useful? |
|-----------|---------|-----------------|
| `const char *str` | pointer to `argv` data | **Yes** — must not modify CLI strings |
| `long max_val` | copy of a number | optional style only; caller unaffected |
| `const char *name` | string literal | **Yes** — error label is read-only |

---

## Worked example: errors

```bash
./ft_ping -c abc host     # invalid count value: 'abc'
./ft_ping -c 3x host      # invalid count value: '3x'
./ft_ping --ttl 999 host  # TTL value out of range: '999'
```

In each case `optarg` still points at the bad token; `parse_number` prints and `exit(EXIT_FAILURE)` before any ping traffic.

---

## Design summary

| Choice | Reason |
|--------|--------|
| Global `optarg` | POSIX `getopt` API; set by `getopt_long` |
| Use `optarg` inside `handle_option` | valid for this loop iteration only |
| `parse_number(optarg, …)` | one validator for all numeric flags |
| `long` return + cast | matches `strtol`; fields in `t_ping` differ |
| `exit` on bad input | fail fast during CLI parsing (before root check for some paths) |
