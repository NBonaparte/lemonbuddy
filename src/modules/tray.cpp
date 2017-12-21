#include "modules/tray.hpp"
#include "utils/factory.hpp"

#include "modules/meta/base.inl"

POLYBAR_NS

namespace modules {
  template class module<tray_module>;

  tray_module::tray_module(const bar_settings& bar, string name_) : event_module<tray_module>(bar, move(name_)) {
    // Create watcher (will fail if already exists)
    try {
      m_watcher = factory_util::unique<watcher>(m_log);
    } catch (const watcher_error& err) {
      m_log.warn("watcher: %s", err.what());
    }
    
    // no need for labels, etc. because all icons
    // Create host
  }

  void tray_module::stop() {
    // Kill watcher, host
    m_watcher.reset();
    m_host.reset();
    event_module::stop();
  }

  bool tray_module::has_event() {
    // Check if new item/removed item
  }

  bool tray_module::update() {
    // use a queue to communicate with watcher/host
    // subscriber stuff?
    // use ntohl to convert from network byte order to host byte order
    // icon handling?
  }

  string tray_module::get_output() {
    // idk
  }

  bool tray_module::build(builder* builder, const string& tag) const {
    // build commands?
    // encase each icon with commands (refer to icon implementation)
  }

  bool tray_module::input(string&& cmd) {
    // check which event occurred (left/right/middle click)
  }
}

POLYBAR_NS_END
