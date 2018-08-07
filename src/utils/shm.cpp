#ifdef __linux__
#include <linux/memfd.h>
#endif // __linux__

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "errors.hpp"
#include "utils/shm.hpp"

POLYBAR_NS

namespace shm_util {
  shm::shm(size_t len) : m_len(len) {
#ifdef __FreeBSD__
    m_fd = shm_open("/polybar", SHM_ANON, 0600);
    if(m_fd < 0)
      throw system_error("shm_open failed.");
#else // Linux
    m_fd = memfd_create("polybar", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if(m_fd < 0)
      throw system_error("memfd_create failed.");

    // seal to prevent shrinking of file
    if(fcntl(m_fd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
      throw system_error("fcntl failed.");
#endif

    // set size
    if(ftruncate(m_fd, m_len) < 0)
      throw system_error("ftruncate failed.");

    // map memory
    m_mem = mmap(nullptr, m_len, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if(m_mem == MAP_FAILED)
      throw system_error("mmap failed.");

  }

  shm::~shm() noexcept(false) {
    if(m_fd) {
      if(munmap(m_mem, m_len) < 0)
        throw system_error("munmap failed.");
      if(close(m_fd) < 0)
        throw system_error("close failed.");
    }
  }

  int shm::get_fd() const {
    return m_fd;
  }
  void *shm::get_mem() const {
    return m_mem;
  }
}

POLYBAR_NS_END
