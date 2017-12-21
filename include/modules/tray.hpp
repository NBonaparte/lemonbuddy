#pragma once

#include "modules/meta/event_module.hpp"
#include "modules/meta/input_handler.hpp"
#include "adapters/sni.hpp"

POLYBAR_NS
namespace sni {
  class watcher;
  class host;
}

namespace tray {
  using watcher_t = shared_ptr<sni::watcher>;
  using host_t = shared_ptr<sni::host>;

  class tray_module : public event_module<tray_module>, public input_handler {
   public:
    explicit tray_module(const bar_settings&, string);

    void stop();
    bool has_event();
    bool update();
    string get_output();
    bool build(builder* builder, const string& tag) const;

   protected:
    bool input(string&& cmd);

   private:
    watcher_t m_watcher;
    host_t m_host;
  }
}

POLYBAR_NS_END
