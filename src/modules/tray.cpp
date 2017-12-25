#include "modules/tray.hpp"
#include "utils/factory.hpp"

#include "modules/meta/base.inl"

POLYBAR_NS

namespace modules {
  template class module<tray_module>;

  tray_module::tray_module(const bar_settings& bar, string name_) : event_module<tray_module>(bar, move(name_)) {
    // Create watcher on new thread
    m_log.warn("%u", std::this_thread::get_id());
    try {
      m_watcher_thread = thread([&] {
        m_watcher = factory_util::unique<sni::watcher>(m_log, m_queue);		      
      });
    // maybe find a better way to allow the watcher to terminate gracefully (if another exists)
    // use queue to notify when to join()? 
      m_watcher_thread.detach();
    } catch (const watcher_error& err) {
      m_log.warn("watcher: %s", err.what());
    }
    // wait until acquired/lost is in queue, then build host?
    // Create host
    try {
      m_host_thread = thread([&] {
        m_host = factory_util::unique<sni::host>(m_log, true, m_queue);
      });
      m_host_thread.detach();
    } catch (const host_error& err) {
      m_log.warn("host: %s", err.what());
    }
  }

  void tray_module::stop() {
    // Kill watcher, host
    m_watcher.reset();
    m_host.reset();
    event_module::stop();
  }

  bool tray_module::has_event() {
    // Check if new item/removed item
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
    // idk
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
