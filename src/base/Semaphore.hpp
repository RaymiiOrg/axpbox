#pragma once

#include "es40_debug.hpp"
#include <chrono>
#include <mutex>
#include <condition_variable>

class CSemaphore {
public:
  explicit CSemaphore(int n) : m_count(n), m_max(n) {}
  CSemaphore(int n, int max) : m_count(n), m_max(max) {
    if (n < 0 || n > max) {
      throw std::invalid_argument("Invalid initial count for semaphore");
    }
  }

  ~CSemaphore() {}

  void set() {
    std::unique_lock<std::mutex> const lock(m_mutex);
    ++m_count;
    m_cv.notify_one();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_count > 0; });
    --m_count;
  }

  void wait(long milliseconds) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return m_count > 0; })) {
      throw std::runtime_error("TimeoutException");
    }
    --m_count;
  }

  bool tryWait(long milliseconds) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return m_count > 0; })) {
      --m_count;
      return true;
    }
    return false;
  }

private:
  CSemaphore() = delete;
  CSemaphore(const CSemaphore &) = delete;
  CSemaphore &operator=(const CSemaphore &) = delete;

  int m_count;
  int m_max;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};