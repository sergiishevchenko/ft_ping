# Command-line parsing with `getopt_long`

`ft_ping` reads `argc` / `argv` in `parse_args()` (`srcs/main.c`) using **`getopt_long`** from `<getopt.h>`. This page explains that machinery: short flags (`-c`), long flags (`--ttl`), the `struct option` table, custom return codes (`OPT_TTL`), and how the hostname is parsed after all options.

For what each flag *does* on the network, see [FLAGS.md](FLAGS.md). For where parsing fits in startup, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## What problem this solves

When you run:

```bash
sudo ./ft_ping -v -c 3 --ttl 64 8.8.8.8
```

the shell passes the program an array of C strings:

| Index | Value |
|-------|-------|
| `argv[0]` | `"./ft_ping"` (program name) |
| `argv[1]` | `"-v"` |
| `argv[2]` | `"-c"` |
| `argv[3]` | `"3"` |
| `argv[4]` | `"--ttl"` |
| `argv[5]` | `"64"` |
| `argv[6]` | `"8.8.8.8"` |
| `argc` | `7` |

The parser must:

1. Recognize **options** (`-v`, `-c`, `--ttl`, …) and their **arguments** (`3`, `64`).
2. Map each option into fields of `t_ping` (`ping->count`, `ping->ttl`, …).
3. Treat the remaining token (`8.8.8.8`) as the **positional hostname**.

`getopt_long` automates step 1. `handle_option()` and the loop over `optind` implement steps 2 and 3.

---

## `getopt` vs `getopt_long`

| API | Supports | Example |
|-----|----------|---------|
| `getopt` | **Short** options only (one leading `-`) | `-c 3`, `-v` |
| `getopt_long` | Short **and long** (`--name`) | `-c 3`, `--ttl 64`, `--help` |

`ft_ping` uses `getopt_long` because inetutils `ping` exposes long options such as `--ttl` and `--ip-timestamp`, while most everyday flags stay short (`-c`, `-v`).

### Function prototype

```c
#include <getopt.h>

int getopt_long(int argc, char * const argv[],
                const char *optstring,
                const struct option *longopts,
                int *longindex);
```

| Parameter | In `ft_ping` | Role |
|-----------|--------------|------|
| `argc`, `argv` | from `main` | Full command line |
| `optstring` | `"c:fl:np:rs:T:vw:W:?"` | Short-option grammar |
| `longopts` | `g_long_opts` | Table of `--long` names |
| `longindex` | `NULL` | Optional index into `longopts`; not needed here |

**Return value:**

- The **option code** for the current token (see below).
- `-1` when there are no more options.

Call it in a loop until it returns `-1`.

---

## Global state used by `getopt_long`

`getopt_long` is stateful. It uses these globals (declared in `<unistd.h>` / `<getopt.h>`):

| Global | Type | Meaning |
|--------|------|---------|
| `optarg` | `char *` | Pointer to the **argument** for the current option (e.g. `"3"` after `-c`) |
| `optind` | `int` | Index of the **next** `argv` element to process; starts at `1`, advances each call |
| `optopt` | `int` | Invalid option character when `getopt_long` returns `'?'` |
| `opterr` | `int` | If non-zero, libc prints its own errors; `ft_ping` sets `opterr = 0` for custom messages |

Detailed coverage of **`optarg`** (lifetime, `argv` trace, link to `parse_number`): [OPTARG.md](OPTARG.md).

After the option loop finishes, **`optind` points at the first non-option argument** — usually the hostname.

```c
/* After parsing all flags */
while (optind < argc)
{
    resolve_host(ping, argv[optind]);  /* positional host */
    optind++;
}
```

---

## Short options: the `optstring`

```c
getopt_long(argc, argv, "c:fl:np:rs:T:vw:W:?", g_long_opts, NULL);
```

The third argument is a compact grammar for **single-letter** flags:

| Char in string | Meaning |
|----------------|---------|
| letter alone (`f`, `n`, `v`, `r`) | flag, **no** argument |
| letter + `:` (`c:`, `s:`) | flag **with required** argument |
| `?` | help (`-?`) |

Full mapping in `ft_ping`:

| Short | `optstring` | Argument | Handler sets |
|-------|-------------|----------|--------------|
| `-c` | `c:` | count | `ping->count` |
| `-f` | `f` | — | flood mode |
| `-l` | `l:` | preload | `ping->preload` |
| `-n` | `n` | — | (accepted, no field) |
| `-p` | `p:` | hex pattern | `ping->pattern` |
| `-r` | `r` | — | `g_dontroute = 1` |
| `-s` | `s:` | size | `ping->data_length` |
| `-T` | `T:` | TOS | `ping->tos` |
| `-v` | `v` | — | `OPT_VERBOSE` |
| `-w` | `w:` | seconds | `ping->timeout` |
| `-W` | `W:` | seconds | `ping->linger` |
| `-?` | `?` | — | print help, exit 0 |

**Note:** `--ttl` is **not** in this string. Long-only options live only in `g_long_opts`.

### Combining short flags

`getopt_long` allows `-fl` as shorthand for `-f -l` **only if** the clustered letters do not require arguments in the middle. Here `-l` needs an argument, so users write `-f -l 5`, not `-fl5` in the usual ping style.

---

## Long options: `struct option` and `g_long_opts`

Long names are described by an array of `struct option`:

```c
struct option {
    const char *name;       /* "--" name without dashes */
    int         has_arg;    /* no_argument | required_argument | optional_argument */
    int        *flag;       /* if non-NULL, store val here and return 0 */
    int         val;        /* return value if flag is NULL */
};
```

`ft_ping` definition:

```c
enum {
    OPT_TTL = 256,
    OPT_IPTS,
};

static struct option g_long_opts[] = {
    {"ttl",           required_argument, NULL, OPT_TTL},
    {"ip-timestamp",  required_argument, NULL, OPT_IPTS},
    {"help",          no_argument,       NULL, '?'},
    {NULL,            0,                 NULL, 0}
};
```

### Row-by-row

| `name` | `has_arg` | `flag` | `val` | CLI example | `getopt_long` returns |
|--------|-----------|--------|-------|-------------|------------------------|
| `"ttl"` | `required_argument` | `NULL` | `OPT_TTL` (256) | `--ttl 32` | `256` |
| `"ip-timestamp"` | `required_argument` | `NULL` | `OPT_IPTS` (257) | `--ip-timestamp tsonly` | `257` |
| `"help"` | `no_argument` | `NULL` | `'?'` | `--help` | `'?'` (same as `-?`) |
| `{NULL, …}` | — | — | — | end of table | — |

### The `flag` pointer

If the fourth field `flag` were **non-NULL**, `getopt_long` would write `val` into `*flag` and return `0`.  
`ft_ping` sets `flag` to **`NULL`** everywhere, so the function returns `val` directly and `handle_option(ping, opt)` can switch on `opt`.

### Terminator row

```c
{NULL, 0, NULL, 0}
```

marks the end of the array (like `NULL` in a linked list). Without it, `getopt_long` would read past the end.

---

## Why `enum { OPT_TTL = 256, OPT_IPTS }`

Short options return an **ASCII character code**:

| Option | Return |
|--------|--------|
| `-c` | `'c'` (99) |
| `-v` | `'v'` (118) |
| `-?` | `'?'` (63) |

Long options `--ttl` and `--ip-timestamp` have **no single-letter form** in the short string. They need distinct integer codes.

`ft_ping` uses:

```c
OPT_TTL  = 256
OPT_IPTS = 257   /* enum auto-increments */
```

Starting at **256** avoids collision with any ASCII code (0–255). `handle_option` then does:

```c
else if (opt == OPT_TTL)
    ping->ttl = (int)parse_number(optarg, 255, "TTL");
else if (opt == OPT_IPTS)
    /* parse tsonly / tsaddr */
```

Alternative designs (also valid): use `'t'` in `optstring` for TTL (inetutils uses `-t` on some systems) or pick any unused integers — **256+** is a common convention.

---

## End-to-end flow in `parse_args`

```
argv:  ./ft_ping  -v  -c  3  --ttl  64  8.8.8.8
                    │    │   │    │     │      │
                    └────┴───┴────┴─────┘      └── positional (after optind)
                         getopt_long loop
```

```c
void parse_args(t_ping *ping, int argc, char **argv)
{
    int opt;
    int host_found = 0;

    opterr = 0;   /* suppress libc "unknown option" — we print our own */

    while ((opt = getopt_long(argc, argv, "c:fl:np:rs:T:vw:W:?",
                              g_long_opts, NULL)) != -1)
    {
        if (opt == '?') { /* help or invalid option */ ... }
        if (opt == 'r')
            g_dontroute = 1;
        else
            handle_option(ping, opt);
    }

    while (optind < argc) { /* exactly one host */ ... }
}
```

### Step 1 — option loop

Each iteration:

1. `getopt_long` consumes the next flag (and its argument if any).
2. `opt == '?'` — help or error (see below).
3. `opt == 'r'` — special case: sets global `g_dontroute` (used in `socket.c`), not only `t_ping`.
4. Otherwise `handle_option(ping, opt)` updates `t_ping`.

### Step 2 — positional hostname

When the loop ends (`opt == -1`), `optind` is `6` in the example above → `argv[6]` is `"8.8.8.8"`.

Rules:

- **Exactly one** host allowed; a second positional prints `only one host allowed`.
- **At least one** host required; missing host → `missing host operand`.
- `resolve_host()` fills `ping->hostname`, `ping->dest_addr`, `ping->ip_str`.

---

## `handle_option` — mapping codes to `t_ping`

Central dispatcher for every recognized option code (short or long):

```c
static void handle_option(t_ping *ping, int opt)
{
    if (opt == 'c')
        ping->count = (size_t)parse_number(optarg, LONG_MAX, "count");
    else if (opt == 'f') { ... }
    /* ... all short flags ... */
    else if (opt == OPT_TTL)
        ping->ttl = (int)parse_number(optarg, 255, "TTL");
    else if (opt == OPT_IPTS) { ... }
}
```

`optarg` is set by `getopt_long` before `handle_option` runs — e.g. for `--ttl 64`, `optarg` points to `"64"`. See [OPTARG.md](OPTARG.md).

Long-option arguments can use `=` form as well:

```bash
./ft_ping --ttl=64 8.8.8.8    # equivalent to --ttl 64
```

---

## Help and error handling (`opt == '?'`)

```c
if (opt == '?')
{
    if (optopt && optopt != '?')
    {
        fprintf(stderr, "ft_ping: invalid option -- '%c'\n", optopt);
        print_usage();
        exit(EXIT_FAILURE);
    }
    print_usage();
    exit(EXIT_SUCCESS);
}
```

| Situation | `optopt` | Result |
|-----------|----------|--------|
| `-?` or `--help` | `0` or `'?'` | print usage, **exit 0** (no root needed) |
| unknown `-z` | `'z'` | invalid option message, usage, **exit failure** |

`opterr = 0` disables duplicate messages from libc; `ft_ping` controls all user-facing text.

---

## Worked examples

### Example A — short flags only

```bash
sudo ./ft_ping -v -c 2 127.0.0.1
```

| Call | Returns | `optarg` |
|------|---------|----------|
| 1st | `'v'` | unused |
| 2nd | `'c'` | `"2"` |
| 3rd | `-1` | — |

Then `optind` → `argv[3]` = `"127.0.0.1"`.

### Example B — long TTL

```bash
sudo ./ft_ping --ttl 32 -c 1 8.8.8.8
```

| Call | Returns | `optarg` |
|------|---------|----------|
| 1st | `OPT_TTL` (256) | `"32"` |
| 2nd | `'c'` | `"1"` |
| 3rd | `-1` | — |

`handle_option` sets `ping->ttl = 32`, then `ping->count = 1`.

### Example C — help without root

```bash
./ft_ping --help
```

Returns `'?'` before any socket or `getuid()` check → usage printed → exit 0. This is intentional so help works without `sudo`.

---

## Design choices in `ft_ping`

| Choice | Reason |
|--------|--------|
| `getopt_long` | Match inetutils long options (`--ttl`, `--ip-timestamp`) |
| `OPT_TTL = 256` | Safe distinct codes for long-only options |
| `--help` → `'?'` | Reuse same branch as `-?` |
| `opterr = 0` | Custom error strings with `ft_ping:` prefix |
| `-r` outside `handle_option` | Sets file-level `g_dontroute` for `socket.c` |
| Host after options | Standard POSIX pattern; `optind` marks start |
| `parse_args` before root check | Help and arg errors without requiring privileges |

---

## Quick reference

| Item | Location / value |
|------|------------------|
| Short string | `"c:fl:np:rs:T:vw:W:?"` |
| Long table | `g_long_opts[]` in `srcs/main.c` |
| Custom codes | `OPT_TTL` (256), `OPT_IPTS` (257) |
| Dispatcher | `handle_option()` |
| Entry point | `parse_args()` |
| Hostname | `argv[optind]` after option loop |
| Header | `<getopt.h>` (included via `ft_ping.h` or system headers) |
