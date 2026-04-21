# C-Shell

A custom Unix-like shell implemented in C for the OSN mini project.

## Features

- Interactive prompt in the format `<user@system:cwd>`.
- Foreground and background process execution.
- Pipeline support using `|`.
- Input and output redirection:
  - `<` for input redirection
  - `>` for overwrite output redirection
  - `>>` for append output redirection
- Multiple commands in one line using separators like `;` and `&`.
- Background job tracking and completion notifications.
- Signal handling for interactive job control:
  - `Ctrl+C` forwards `SIGINT` to the foreground job
  - `Ctrl+Z` stops the foreground job (`SIGTSTP`)
- Persistent command history stored in `~/.shell_history` (up to 15 commands).

## Built-in Commands

### `hop`
Change directory (similar to `cd`).

Usage examples:
- `hop` -> go to shell home directory
- `hop ~` -> go to shell home directory
- `hop ..` -> go to parent directory
- `hop -` -> go to previous directory
- `hop <path>` -> go to specific directory

### `reveal`
List directory contents (similar to `ls`).

Flags:
- `-a` show hidden files
- `-l` print one entry per line

Usage examples:
- `reveal`
- `reveal -a`
- `reveal -l`
- `reveal -al /some/path`
- `reveal - <or> reveal ~ <or> reveal ..`

### `log`
Command history operations.

Usage:
- `log` -> print history
- `log purge` -> clear history
- `log execute <index>` -> execute a command from history (`1` = most recent)

### `activities`
Display active background jobs, sorted lexicographically by command name, with running/stopped state.

Usage:
- `activities`

### `ping`
Send a signal to a process.

Usage:
- `ping <pid> <signal_number>`

Notes:
- Signal used is `signal_number % 32`.

### `fg`
Bring a background job to foreground.

Usage:
- `fg` -> most recent job
- `fg <job_number>` or `fg %<job_number>`

### `bg`
Resume a stopped job in background.

Usage:
- `bg` -> most recent job
- `bg <job_number>` or `bg %<job_number>`

### `exit` and `Ctrl+D`
- `exit` terminates the shell.
- `Ctrl+D` prints `logout`, kills active background jobs, and exits.

## Build and Run (Using Makefile)

### Prerequisites

- Linux/Unix environment
- `gcc`
- `make`

### Build

```bash
make
```

This compiles all source files and produces:

- `shell.out`

### Run

```bash
./shell.out
```

### Clean build artifacts

```bash
make clean
```

This removes object files and the executable.

## Quick Start

```bash
make
./shell.out
```

Inside the shell, try:

```bash
reveal
hop ..
sleep 20 &
activities
fg
```

## Project Structure

Core modules include:

- `main.c`, `shell.c` -> shell initialization and main loop
- `parser.c` -> command syntax parsing
- `execute.c` -> command execution, pipes, redirection, background jobs
- `prompt.c` -> prompt formatting
- `hop.c`, `reveal.c`, `log.c`, `activities.c`, `ping.c`, `fg_bg.c` -> built-ins
- `signal_handlers.c` -> Ctrl+C/Ctrl+Z/EOF handling
