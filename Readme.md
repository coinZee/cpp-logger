# Fast cpp logger

A minimal, header only low-latency C++ logger using `mmap` and a background worker thread to avoid blocking the main execution.

### Features
* **Non-blocking:** The main thread pushes tasks to a queue. and a worker thread handles disk I/O. Also no mutexes.
* **Zero-Copy I/O:** Uses linux `mmap` for direct memory access instead of slow file streams.
* **Thread Safe:** Thread safe.

### Compatibility
* **Linux / macOS** (Native support via POSIX)
* **Windows** (Requires WSL or code changes)

### Build & Run
```bash
# Compile (requires pthread)
g++ example.cpp -o logger -pthread

# Run
./logger
