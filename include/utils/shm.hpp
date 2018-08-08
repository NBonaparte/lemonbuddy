#pragma once

#include "common.hpp"

POLYBAR_NS

namespace shm_util {
  int create_shm(size_t len);
  void resize_shm(int fd, size_t len);
  void destroy_shm(int fd);
}

POLYBAR_NS_END
