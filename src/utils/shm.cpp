#if defined(__linux__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 27
#include <linux/memfd.h>
#endif // __linux__

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "errors.hpp"
#include "utils/shm.hpp"

POLYBAR_NS

namespace shm_util {
  int create_shm(size_t len) {
#ifdef __FreeBSD__
    auto fd = shm_open("/polybar", SHM_ANON, 0600);
    if(fd < 0)
      throw system_error("failed to create shared memory object using shm_open");
#elif defined(__linux__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 27
    // requires glibc>=2.27 (or use syscall instead)
    auto fd = memfd_create("polybar", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if(fd < 0)
      throw system_error("failed to create anonymous file using memfd_create");

    // seal to prevent shrinking of file
    if(fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
      throw system_error("failed to seal anonymous file");
#else
    // use standard mkstemp and unlink (posix)
    char name[] = "/tmp/polybarXXXXXX";
    auto fd = mkstemp(name);
    if (fd < 0)
      throw system_error("failed to create temporary file using mkstemp");

    auto ret = unlink(name);
    if (ret < 0)
      throw system_error("failed to unlink temporary file");
#endif

    // set size
    if(ftruncate(fd, len) < 0)
      throw system_error("failed to set shared memory object size");

    return fd;
  }

  /*
   * Resize shared memory file by `len` bytes.
   */
  void resize_shm(int fd, size_t len) {
    auto orig_pos = lseek(fd, 0, SEEK_CUR);
    auto fsize = lseek(fd, 0, SEEK_END);
    if (ftruncate(fd, fsize + len) < 0) {
      throw system_error("failed to resize shared memory object");
    }
    // reset offset
    lseek(fd, orig_pos, SEEK_SET);
  }

  void destroy_shm(int fd) {
    if (close(fd) < 0) {
      throw system_error("failed to close shared memory object");
    }
  }
}

POLYBAR_NS_END
