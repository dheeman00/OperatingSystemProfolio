# 🛠️ Project Portfolio

This repository contains two system-level programming projects that demonstrate distributed systems and Unix shell design. Each project explores a different core aspect of operating systems and concurrency.

---

## 🔐 Chubby Lock Server (Go)

**Overview:**
A lightweight distributed lock server inspired by Google's Chubby lock service. It is built in **Go** and extends a Raft-based key-value store for fault-tolerant, replicated coordination.

**Key Features:**
- Built on top of [otoolep/hraftd](https://github.com/otoolep/hraftd), which implements the Raft consensus algorithm using HashiCorp's Raft library
- Supports distributed locks with consistent state replication
- Exposes a simple HTTP interface for acquiring and releasing locks
- Stateless client interaction via `curl` and REST endpoints

**Use Cases:**
- Coordinating access to shared resources in distributed applications
- Leader election and consensus-backed locking mechanisms

**Technologies:** Go, Raft (HashiCorp), HTTP

---

## 🐚 ish Shell (C)

**Overview:**
A Unix-like shell implementation written in **C**, supporting job control, I/O redirection, environment handling, and pipelining. Inspired by early Unix shell designs.

**Key Features:**
- Command parsing and execution with support for foreground/background jobs
- Built-in support for Unix pipes (`|`) and redirection (`<`, `>`, `>>`)
- Process management via fork/exec and signal handling
- Compatible with Linux/GNU toolchains and standard shell environments

**Development Tools:**
- Optional use of `lex` and `yacc` (or `flex`/`bison`) for building the command parser
- Clean and portable compilation using `make`

**Technologies:** C, Unix system calls, Make, lex/yacc

---

Each of these projects can be run independently and provide foundational components that could be integrated into larger system-level applications.
