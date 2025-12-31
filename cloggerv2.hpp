#include <atomic>
#include <cstring>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class Clogger {
public:
  Clogger(const char *fp);
  ~Clogger();
  void log(std::string_view data);

private:
  static constexpr size_t RING_SIZE = 4 * 1024 * 1024; // 4Mb buffer

  alignas(64) std::atomic<size_t> m_head{0}; // producer cacheline
  std::vector<char> m_ring; 

  alignas(64) std::atomic<size_t> m_tail{0}; // consumer cacheline
  
  std::atomic<bool> m_running{true}; // stuff touched by consumer
  char *wdata;
  size_t m_cursor;
  size_t fsize;

  std::thread m_thread; // touched by nothing 2 spawns and be done
  int fd; // file descriptor

  void workerLoop();
  void resizeLog();
};

inline Clogger::Clogger(const char *fp) : m_ring(RING_SIZE) {
  fd = open(fp, O_RDWR | O_CREAT, 0644);
  if (fd == -1) {
    std::cerr << "fd err\n";
    return;
  }

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    std::cerr << "fstat err\n";
    close(fd);
    return;
  }

  m_cursor = sb.st_size; // keep the cursor at 0 by default

  // check if the file is empty then reserve 4kb  
  if (sb.st_size == 0) {
    fsize = 4096;
    ftruncate(fd, fsize);
  } else {
    fsize = sb.st_size;
  }
  
  wdata = static_cast<char *>(
      mmap(nullptr, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  
  if (wdata == MAP_FAILED) {
    std::cerr << "mmap err\n";
    close(fd);
    return;
  }

  m_thread = std::thread(&Clogger::workerLoop, this);
}

inline Clogger::~Clogger() {
  m_running.store(false, std::memory_order_release);
  
  if (m_thread.joinable()) m_thread.join();
  
  if (wdata && wdata != MAP_FAILED) munmap(wdata, fsize);
  
  if (fd != -1) {
    // trim file to exact size of data that is written
    ftruncate(fd, m_cursor);
    close(fd);
  }
}

inline void Clogger::resizeLog() {
  munmap(wdata, fsize);
  size_t new_size = fsize * 2;
  ftruncate(fd, new_size);
  
  wdata = static_cast<char *>(
      mmap(nullptr, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

  if (wdata == MAP_FAILED) {
    std::cerr << "resize failed\n";
    exit(1);
  }
  fsize = new_size;
}

inline void Clogger::log(std::string_view data) {
  size_t msg_len = data.size();
  size_t packet_len = sizeof(size_t) + msg_len;

  size_t head = m_head.load(std::memory_order_relaxed);
  size_t tail = m_tail.load(std::memory_order_acquire);

  size_t free_space = (head >= tail) 
      ? RING_SIZE - (head - tail) 
      : tail - head;

  if (free_space <= packet_len + 1) {
    return; // drop the log because...
  }

  for (size_t i = 0; i < sizeof(size_t); ++i) {
    m_ring[(head + i) % RING_SIZE] = reinterpret_cast<const char*>(&msg_len)[i];
  }

  size_t data_start = (head + sizeof(size_t)) % RING_SIZE;
  if (data_start + msg_len <= RING_SIZE) {
    memcpy(&m_ring[data_start], data.data(), msg_len);
  } else {
    size_t first_part = RING_SIZE - data_start;
    memcpy(&m_ring[data_start], data.data(), first_part);
    memcpy(&m_ring[0], data.data() + first_part, msg_len - first_part);
  }

  m_head.store((head + packet_len) % RING_SIZE, std::memory_order_release);
}

inline void Clogger::workerLoop() {
  std::vector<char> scratch; 
  scratch.reserve(4096); 

  while (true) {
    size_t tail = m_tail.load(std::memory_order_relaxed);
    size_t head = m_head.load(std::memory_order_acquire);

    if (head == tail) {
      if (!m_running.load(std::memory_order_acquire)) return;
      std::this_thread::sleep_for(std::chrono::microseconds(10)); // dont be hard on ur cpu <3
      continue;
    }

    size_t msg_len;
    for (size_t i = 0; i < sizeof(size_t); ++i) {
      reinterpret_cast<char*>(&msg_len)[i] = m_ring[(tail + i) % RING_SIZE];
    }

    if (scratch.size() < msg_len) scratch.resize(msg_len);
    
    size_t data_start = (tail + sizeof(size_t)) % RING_SIZE;
    if (data_start + msg_len <= RING_SIZE) {
      memcpy(scratch.data(), &m_ring[data_start], msg_len);
    } else {
      size_t first_part = RING_SIZE - data_start;
      memcpy(scratch.data(), &m_ring[data_start], first_part);
      memcpy(scratch.data() + first_part, &m_ring[0], msg_len - first_part);
    }

    if (m_cursor + msg_len + 1 > fsize) {
      resizeLog();
    }
    
    memcpy(wdata + m_cursor, scratch.data(), msg_len);
    m_cursor += msg_len;
    
    if (m_cursor < fsize) {
        wdata[m_cursor] = '\n';
        m_cursor++;
    }

    m_tail.store((tail + sizeof(size_t) + msg_len) % RING_SIZE, std::memory_order_release);
  }
}

// int main() {
//   Clogger logger{"log.txt"};
//   for(int i=0; i<1000; ++i) logger.log("Test Line " + std::to_string(i));
//   return 0;
// }
