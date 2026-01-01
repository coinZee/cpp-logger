# Fast c++ logger v2

A simple, fast C++ logger for Linux/macOS. It uses a **Ring Buffer** and **mmap** to write logs without blocking your main thread.

**Requires:** C++17 (for `string_view`).

### How it works
* **No blocking:** The main thread just copies bytes into a buffer and keeps moving. No `malloc`, no `new` or waiting for the disk.
* **No locks:** Uses atomic indices (Head/Tail) to manage the buffer.
* **mmap:** Writes directly to memory instead of using slow file streams (`fstream`).
* **Cache friendly:** Variables are aligned to 64-byte cache lines <3

### Compatibility
* **Linux / macOS:** Works natively.
* **Windows:** No (probably wsl).

### Build & Run
```bash
# Compile
g++ main.cpp -std=c++17 -pthread -o logger # dont need -O2 or -O3 they failed miserably

# Run
./logger
