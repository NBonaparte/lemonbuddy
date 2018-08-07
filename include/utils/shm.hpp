#pragma once

#include "common.hpp"

POLYBAR_NS

namespace shm_util {
  class shm {
    public:
      shm(size_t len);
      ~shm() noexcept(false);
      int get_fd() const;
      void *get_mem() const;
    private:
      int m_fd{0};
      size_t m_len{0};
      void *m_mem{nullptr};
  };
}

POLYBAR_NS_END
