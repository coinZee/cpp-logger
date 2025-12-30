// #include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class Clogger {
public:
  Clogger(const char *fp); // file path
  ~Clogger();
  void log(std::string_view data);

private:
  const char *fp;
  int fd; // file descriptor
  size_t fsize;

  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::queue<std::string> m_tasks;
  bool m_running{true};
  std::thread m_thread;
  size_t m_cursor;
  char *wdata;
  std::vector<std::string_view> m_vec;

  void workerLoop();
  void resizeLog();
};

Clogger::Clogger(const char *fp) : fp(fp) {
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

  if (sb.st_size == 0) {
    ftruncate(fd, 4096);
    fsize = 4096;
  } else {
    fsize = sb.st_size;
  }

  m_cursor = fsize;

  wdata = static_cast<char *>(
      mmap(nullptr, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  if (wdata == MAP_FAILED) {
    std::cerr << "mmap err\n";
    close(fd);
    return;
  }
  m_thread = std::thread(&Clogger::workerLoop, this);
}
Clogger::~Clogger() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = false;
  }
  m_cv.notify_one();
  if (m_thread.joinable()) m_thread.join();
  if (wdata && wdata != MAP_FAILED) munmap(wdata, fsize);
  if (fd != -1)
  {
    ftruncate(fd, m_cursor);
    close(fd);
  }
}

void Clogger::resizeLog()
{
  munmap(wdata, fsize);
  size_t new_size = fsize * 2;
  ftruncate(fd, new_size);
  wdata = static_cast<char*>(mmap(nullptr, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  
  if (wdata == MAP_FAILED) {
    std::cerr << "resize failed\n";
    exit(1);
  }
  fsize = new_size;
}

void Clogger::log(std::string_view data) {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tasks.emplace(data);
  }
  m_cv.notify_one();
}

void Clogger::workerLoop() {
  while (true) {
    std::string current_data;

    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_cv.wait(lock, [this] { return !m_tasks.empty() || !m_running; });
      if (!m_running && m_tasks.empty())
        return;
      if (m_tasks.empty())
        continue;
      current_data = m_tasks.front();
      m_tasks.pop();
    }

    {
      if (m_cursor + current_data.size() > fsize) {
        resizeLog();
      }
      memcpy(wdata + m_cursor, current_data.data(), current_data.size());
      m_cursor += current_data.size();

      if (m_cursor < fsize) {
        wdata[m_cursor] = '\n';
        m_cursor++;
      }
    }
  }
}


// example usage
/*
int main() {
  Clogger logger{"txt_file.txt"};

  for (int i = 0; i < 1000; ++i) {
    logger.log("Hello Log " + std::to_string(i));
  }
  return 0;
}
*/
