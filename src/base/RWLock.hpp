#ifndef Foundation_RWLock_INCLUDED
#define Foundation_RWLock_INCLUDED

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdio>
#include <thread>
#include "es40_debug.hpp"
class CScopedRWLock;

class CRWLock {
public:
  typedef CScopedRWLock CScopedLock;

  CRWLock() =default;

  ~CRWLock() =default;

  void readLock();
  bool tryReadLock();
  void writeLock();
  bool tryWriteLock();
  void unlock();

  void readLock(long milliseconds);
  bool tryReadLock(long milliseconds);
  void writeLock(long milliseconds);
  bool tryWriteLock(long milliseconds);

private:
  std::mutex mutex;
  std::condition_variable cv_reader;
  std::condition_variable cv_writer;
  int readers;
  int writers;
  int active_writers;

  char* lockName;

  void readLockImpl();
  bool tryReadLockImpl();
  void writeLockImpl();
  bool tryWriteLockImpl();
  void unlockImpl();
};

class CScopedRWLock {
public:
  CScopedRWLock(CRWLock* rwl, bool write = false);
  ~CScopedRWLock();

private:
  CRWLock* _rwl;
};

inline void CRWLock::readLock() {
  std::unique_lock<std::mutex> lk(mutex);
  while (writers > 0 || active_writers > 0) {
    cv_reader.wait(lk);
  }
  ++readers;
}

inline bool CRWLock::tryReadLock() {
  std::unique_lock<std::mutex> lk(mutex);
  if (writers > 0 || active_writers > 0) {
    return false;
  }
  ++readers;
  return true;
}

inline void CRWLock::writeLock() {
  std::unique_lock<std::mutex> lk(mutex);
  ++writers;
  while (readers > 0 || active_writers > 0) {
    cv_writer.wait(lk);
  }
  --writers;
  ++active_writers;
}

inline bool CRWLock::tryWriteLock() {
  std::unique_lock<std::mutex> lk(mutex);
  if (readers > 0 || active_writers > 0) {
    return false;
  }
  ++writers;
  ++active_writers;
  return true;
}

inline void CRWLock::unlock() {
  std::lock_guard<std::mutex> lk(mutex);
  if (--readers == 0 && writers > 0) {
    cv_writer.notify_one();
  }
  else if (active_writers > 0) {
    cv_reader.notify_all();
  }
}

inline void CRWLock::readLock(long milliseconds) {
  std::unique_lock<std::mutex> lk(mutex);
  if (writers > 0 || active_writers > 0) {
    if (cv_reader.wait_for(lk, std::chrono::milliseconds(milliseconds)) == std::cv_status::timeout) {
      FAILURE("Timeout", "Timeout");
    }
  }
  ++readers;
}

inline bool CRWLock::tryReadLock(long milliseconds) {
  std::unique_lock<std::mutex> lk(mutex);
  if (writers > 0 || active_writers > 0) {
    if (cv_reader.wait_for(lk, std::chrono::milliseconds(milliseconds)) == std::cv_status::timeout) {
      return false;
    }
  }
  ++readers;
  return true;
}

inline void CRWLock::writeLock(long milliseconds) {
  std::unique_lock<std::mutex> lk(mutex);
  if (readers > 0 || active_writers > 0) {
    if (cv_writer.wait_for(lk, std::chrono::milliseconds(milliseconds)) == std::cv_status::timeout) {
      FAILURE("Timeout", "Timeout");
    }
  }
  --writers;
  ++active_writers;
}

inline bool CRWLock::tryWriteLock(long milliseconds) {
  std::unique_lock<std::mutex> lk(mutex);
  if (readers > 0 || active_writers > 0) {
    if (cv_writer.wait_for(lk, std::chrono::milliseconds(milliseconds)) == std::cv_status::timeout) {
      return false;
    }
  }
  --writers;
  ++active_writers;
  return true;
}

inline void CRWLock::readLockImpl() {
  std::unique_lock<std::mutex> lk(mutex);
  while (writers > 0 || active_writers > 0) {
    cv_reader.wait(lk);
  }
  ++readers;
}

inline bool CRWLock::tryReadLockImpl() {
  std::unique_lock<std::mutex> lk(mutex);
  if (writers > 0 || active_writers > 0) {
    return false;
  }
  ++readers;
  return true;
}

inline void CRWLock::writeLockImpl() {
  std::unique_lock<std::mutex> lk(mutex);
  ++writers;
  while (readers > 0 || active_writers > 0) {
    cv_writer.wait(lk);
  }
  --writers;
  ++active_writers;
}

inline bool CRWLock::tryWriteLockImpl() {
  std::unique_lock<std::mutex> lk(mutex);
  if (readers > 0 || active_writers > 0) {
    return false;
  }
  ++writers;
  ++active_writers;
  return true;
}

inline void CRWLock::unlockImpl() {
  std::lock_guard<std::mutex> lk(mutex);
  if (--readers == 0 && writers > 0) {
    cv_writer.notify_one();
  }
  else if (active_writers > 0) {
    cv_reader.notify_all();
  }
}

inline CScopedRWLock::CScopedRWLock(CRWLock* rwl, bool write) : _rwl(rwl) {
  if (write)
    _rwl->writeLock();
  else
    _rwl->readLock();
}

inline CScopedRWLock::~CScopedRWLock() {
  _rwl->unlock();
}

#define SCOPED_READ_LOCK(mutex) CRWLock::CScopedLock L_##__LINE__(mutex, false)
#define SCOPED_WRITE_LOCK(mutex) CRWLock::CScopedLock L_##__LINE__(mutex, true)

#endif // Foundation_RWLock_INCLUDED
