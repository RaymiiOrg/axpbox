#pragma once

#include <mutex>
#include <chrono>
#include <stdexcept>
#include "es40_debug.hpp"


#if defined(NO_LOCK_TIMEOUTS)
#define LOCK_TIMEOUT_MS
#else
#if !defined(LOCK_TIMEOUT_MS)
#define LOCK_TIMEOUT_MS 5000
#endif
#endif

class CMutex {
public:
  CMutex() {}
  ~CMutex() {}

  void lock() { m_mutex.lock(); }

  void lock(long milliseconds) {
    if (!m_mutex.try_lock_for(std::chrono::milliseconds(milliseconds))) {
      FAILURE("Timeout", "Timeout");
    }
  }

  bool tryLock() { return m_mutex.try_lock(); }

  bool tryLock(long milliseconds) {
    return m_mutex.try_lock_for(std::chrono::milliseconds(milliseconds));
  }

  void unlock() { m_mutex.unlock(); }

private:
  std::timed_mutex m_mutex;
};

class CFastMutex {
public:
  CFastMutex() {}
  ~CFastMutex() {}

  void lock() { m_mutex.lock(); }

  void lock(long milliseconds) {
    if (!m_mutex.try_lock_for(std::chrono::milliseconds(milliseconds))) {
      throw std::runtime_error("TimeoutException");
    }
  }

  bool tryLock() { return m_mutex.try_lock(); }

  bool tryLock(long milliseconds) {
    return m_mutex.try_lock_for(std::chrono::milliseconds(milliseconds));
  }

  void unlock() { m_mutex.unlock(); }

private:
  std::timed_mutex m_mutex;
};

#define MUTEX_LOCK(mutex) mutex->lock(LOCK_TIMEOUT_MS)
#define MUTEX_UNLOCK(mutex) mutex->unlock()
