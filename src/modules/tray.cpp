#include "modules/tray.hpp"
#include "utils/factory.hpp"

#include "modules/meta/base.inl"

POLYBAR_NS

namespace modules {
  template class module<tray_module>;

  tray_module::tray_module(const bar_settings& bar, string name_) : event_module<tray_module>(bar, move(name_)) {
    // Create host
    try {
      m_host = factory_util::unique<sni::host>(m_log, false, m_queue);
    } catch (const host_error& err) {
      m_log.warn("host: %s", err.what());
    }
  }

  void tray_module::stop() {
    m_host.reset();
    event_module::stop();
  }

  bool tray_module::has_event() {
    // Check if new item/removed item
    sni::evtype item;
    bool event = m_queue.try_dequeue(item);
    if (!event) {
      return false;
    }

    switch (item) {
      case sni::evtype::HOST_ITEM:
      case sni::evtype::HOST_CHANGE:
      case sni::evtype::HOST_INIT:
        m_log.warn("%u", m_host->get_items().size());
        return true;
      default:
        break;
    }
    return false;
  }

  bool tray_module::update() {
    // use a queue to communicate with watcher/host
    // subscriber stuff?
    // use ntohl to convert from network byte order to host byte order
    // icon handling?
    return true;
  }

  string tray_module::get_output() {
    return ""s;
  }

  bool tray_module::build(builder* builder, const string& tag) const {
    (void) builder;
    (void) tag;
    // build commands?
    // encase each icon with commands (refer to icon implementation)
    return true;
  }

  bool tray_module::input(string&& cmd) {
    (void) cmd;
    // check which event occurred (left/right/middle click)
    return true;
  }
}

POLYBAR_NS_END
