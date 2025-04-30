# 🐚 `ish` — A Unix-like Shell Implementation in C

## 🔍 Overview

This project implements a Unix-like command-line shell called `ish`, inspired by the original Unix shell described by Ritchie and Thompson. It supports core shell functionalities including job control, I/O redirection, environment handling, and pipelines.

The implementation is written in **C** (or optionally **C++**) and designed to be compiled using a standard `Makefile`. The final executable is named `ish`. The codebase is organized, warning-free, and built for compatibility with standard Linux environments.

## ⚙️ Features

- Execution of foreground and background commands
- Built-in command support
- Job control and process group management
- I/O redirection (`<`, `>`, `>>`)
- Pipelining (`|`)
- Signal handling
- Environment variable management

## 📂 Repository Contents

- `ish.c` — Driver for the shell
- `lexer.l`, `parser.y` — Optional front-end components for parsing shell commands using `lex` and `yacc`
- `command.c`, `command.h` — Defines the shell command structure and parsing interface
- `Makefile` — For building the shell and parser components
- `tutorial.pdf` — Brief tutorial on using `lex` and `yacc`
- `testcases/` — Scripts and programs to validate shell behavior

## 🚀 Getting Started

### 1. Build the Shell
Ensure you are on a Linux system with `gcc`, `make`, `lex`, and `yacc` (or `flex` and `bison`) installed. Then run:

```bash
make
```

This compiles the shell and generates an executable named `ish`.

### 2. Run `ish`

```bash
./ish
```

Enter commands just like a traditional shell. Use `Ctrl+C` to interrupt and `Ctrl+Z` to suspend.

## 📖 Reference and Support Materials

- Unix system calls: Section 2 of the UNIX manual
- Standard C library routines: Section 3
- _Advanced Programming in the UNIX Environment_ by Richard Stevens — Chapters 7–9 are particularly useful

## 🛠️ Notes on Provided Files

The repository includes optional starter files that offer a basic command parsing interface. These use `lex` and `yacc` to convert shell input into a structured format accessible via `nextCommand()`. You may use, modify, or discard these files as needed. If unused, please delete them from your copy.

> Note: While reasonable effort has been made to ensure the correctness of these starter files, they may contain bugs or limitations—especially with semantic edge cases like ambiguous redirections.

## 🧠 Development Tips

- Avoid infinite process spawning: excessive forking may crash your system.
- Limit user processes using:
  ```bash
  ulimit -u <number>    # For sh/bash
  limit maxproc <number>  # For csh
  ```
- Keep an extra shell session open to use `kill -9 -1` in emergencies.
- Test extensively. Try commands in `csh` to model expected behavior.
- Build features incrementally: start with simple execution, then add PATH handling, redirection, job control, and finally pipelines.

## 🧪 Testing

A `testcases/` directory is included with scripts to validate shell features. These are intended to verify command execution, job control, I/O, and piping correctness.

## 🧾 License

This project is released under the MIT License. See the [LICENSE](LICENSE) file for details.

---

For any questions or contributions, feel free to open issues or submit pull requests.
