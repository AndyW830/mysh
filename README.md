# 🐚 mysh – Custom Unix Shell & Chat Server

A system programming project implemented in **C**, featuring a Unix-like shell (`mysh`) and a network-based chat server.  
This project demonstrates process management, inter-process communication, signal handling, and socket programming.

---

## ✨ Features

### 🔹 mysh – Custom Unix Shell
- **10+ built-in commands** (`cd`, `exit`, `export`, `pwd`, etc.)
- **I/O redirection**: input `<`, output `>`, append `>>`
- **Pipeline communication**: support for multi-stage pipes  
  Example: `cat input.txt | grep "error" | sort | uniq -c`
- **Background job execution** with `&`
- **Signal handling**: `SIGINT`, `SIGCHLD` for process lifecycle
- **Environment variables**: export and substitution
- **Modular design**: parser, executor, built-ins, variables, and job control
- **Error handling** for invalid syntax and commands

### 🔹 Chat Server (Networking Extension)
- Implemented a **multi-client TCP chat server**
- **Broadcast messaging**: messages from one client are sent to all connected clients
- **Short-lived & persistent clients** tested (`tests_short_client.py`, `tests_long_client.py`)
- **Robust socket management** with error handling and concurrent connections
- Demonstrates **socket programming** and **client-server communication**

---

## 🛠️ Build & Run

### Build
```bash
cd src
make
```

### Run Shell
```bash
./mysh
```

### Run Chat Server
```bash
./server <port>
```

### Connect with Client
```bash
# simple test with netcat
nc localhost <port>

# or use provided Python clients
python tests/milestone5tests/tests_short_client.py
python tests/milestone5tests/tests_long_client.py
```

---

## 📖 Usage Examples (Shell)
```bash
mysh> ls -l | grep ".c" | wc -l
mysh> cat < input.txt | sort | uniq -c
mysh> sleep 10 &
mysh> cd src
mysh> export PATH=$PATH:/usr/local/bin
```

---

## 📖 Usage Examples (Chat Server)
```
Client 1: hello
Client 2: hi there!
# Both clients see each other's messages in real-time
```

---

## 🔍 Architecture
```
project/
├── src/
│   ├── mysh.c           # main shell
│   ├── server.c         # chat server
│   ├── commands.c/h     # command parsing/execution
│   ├── builtins.c/h     # built-in command implementations
│   ├── variables.c/h    # environment variable management
│   ├── io_helpers.c/h   # I/O redirection helpers
│   ├── signals.c/h      # signal handlers
│   ├── jobs.c/h         # background job control
│   └── Makefile

---

## 🚀 Future Improvements
- Shell: command history, tab completion, job control (`jobs`, `fg`, `bg`)
- Chat: authentication, private messages, persistent message logs

---

## 📄 License
This project is released under the MIT License.  
Feel free to use, modify, and share.
