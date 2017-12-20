//#include "sni-generated.h"
#include <gio/gio.h>
#include <algorithm>
#include "adapters/sni.hpp"
#include "components/logger.hpp"
#include "common.hpp"

POLYBAR_NS

/* Some ideas:
 * - Use existing i3 icon infrastructure to render the icons
 * - Make as part of module and let renderer handle drawing
 * - ~~Combine watcher and host into one glib thing~~
 *   - Bad if more than one host -> use 1 watcher, but as many hosts as needed?
 */
namespace sni {
  // run main loop on separate thread, notify hosts for each event?
  watcher::watcher(const logger& logger) : m_log(logger) {
    m_intro_data = g_dbus_node_info_new_for_xml(XML_DATA, nullptr);
    g_assert(m_intro_data != nullptr);

    m_mainloop = g_main_loop_new(nullptr, FALSE);
    m_id = g_bus_own_name(G_BUS_TYPE_SESSION, WATCHER_NAME, G_BUS_NAME_OWNER_FLAGS_REPLACE,
        on_bus_acquired, on_name_acquired, on_name_lost, this, nullptr);
    g_main_loop_run(m_mainloop);
  }

  watcher::~watcher() {
    g_bus_unown_name(m_id);
    g_main_loop_unref(m_mainloop);
  }

  void watcher::item_appeared_handler(GDBusConnection *, const gchar *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    auto item = This->tmp_param;
    This->m_items.emplace_back(item);
    This->m_log.info("tray-watcher: new item %s, managing %d items", item, This->m_items.size());
  }

  void watcher::item_vanished_handler(GDBusConnection *c, const gchar *, gpointer userdata) {
   watcher *This = static_cast<watcher *>(userdata);
   auto item = This->tmp_param;
   This->m_items.erase(std::remove(This->m_items.begin(), This->m_items.end(), item), This->m_items.end());
   This->m_log.info("tray-watcher: removing item %s, managing %d items", item, This->m_items.size());
   g_dbus_connection_emit_signal(c, nullptr, WATCHER_PATH, WATCHER_NAME, "StatusNotifierItemUnregistered",
       g_variant_new("(s)", item.c_str()), nullptr);
  }

  void watcher::host_appeared_handler(GDBusConnection *, const gchar *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    auto host = This->tmp_param;
    This->m_hosts.emplace_back(host);
    This->m_log.info("tray-watcher: new host %s, managing %d hosts", host, This->m_hosts.size());
  }

  void watcher::host_vanished_handler(GDBusConnection *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    auto host = This->tmp_param;
    This->m_hosts.erase(std::remove(This->m_hosts.begin(), This->m_hosts.end(), host), This->m_hosts.end());
    This->m_log.info("tray-watcher: removing host %s, managing %d hosts", host, This->m_hosts.size());
  }

  void watcher::handle_method_call(GDBusConnection *c, const gchar *sender, const gchar *,
      const gchar *, const gchar *method_name, GVariant *param, GDBusMethodInvocation *invoc,
      gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    // check whether xembedsniproxy is being used
    bool proxy = false;
    string param_str, service;
    g_variant_get(param, "(&s)", *(param_str.c_str()));
    This->m_log.info("tray-watcher: %s called method %s, with arguments %s", sender, method_name, param_str);

    if (g_strcmp0(method_name, "RegisterStatusNotifierItem") == 0) {
      if (param_str[0] == '/') {
        // libappindicator sends object path, so we should use sender name + object path
        service = sender + param_str;
      } else if (param_str[0] == ':') {
        // xembedproxy sends item name, so we should use item from argument
	service = param_str + "/StatusNotifierItem";
      } else {
        service = sender;
        service += "/StatusNotifierItem";
      }

      g_dbus_method_invocation_return_value(invoc, nullptr);
      g_dbus_connection_emit_signal(c, nullptr, WATCHER_PATH, WATCHER_NAME, "StatusNotifierItemRegistered",
          g_variant_new("(s)", service.c_str()), nullptr);
      This->tmp_param = service;
      g_bus_watch_name(G_BUS_TYPE_SESSION, proxy ? param_str.c_str() : sender, G_BUS_NAME_WATCHER_FLAGS_NONE,
          item_appeared_handler, item_vanished_handler, (gpointer) This, nullptr);
          // will service disappear?
    } else if (g_strcmp0(method_name, "RegisterStatusNotifierHost") == 0) {
      service = string{sender};

      g_dbus_method_invocation_return_value(invoc, nullptr);
      g_dbus_connection_emit_signal(c, nullptr, WATCHER_PATH, WATCHER_NAME, "StatusNotifierItemRegistered",
          g_variant_new("(s)", sender), nullptr);
      This->tmp_param = service;
      g_bus_watch_name(G_BUS_TYPE_SESSION, sender, G_BUS_NAME_WATCHER_FLAGS_NONE, host_appeared_handler,
          host_vanished_handler, (gpointer) This, nullptr);
    }
  }
  GVariant *watcher::handle_get_property(GDBusConnection *, const gchar *sender, const gchar *,
      const gchar *, const gchar *prop_name, GError **, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    This->m_log.info("tray-watcher: %s requested property %s", sender, prop_name);

    if (g_strcmp0(prop_name, "RegisteredStatusNotifierItems") == 0) {
      // construct gvariant of type 'as' (array of strings) from m_items
      GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
      for (auto&& item : This->m_items) {
        g_variant_builder_add(builder, "s", item.c_str());
      }
      // do g_variant_ref_sink(ret)?
      GVariant *ret = g_variant_new("as", builder);
      g_variant_builder_unref(builder);
      return ret;
    } else if (g_strcmp0(prop_name, "IsStatusNotifierHostRegistered") == 0) {
      return g_variant_new_boolean(This->m_hosts.size() > 0);
    } else if (g_strcmp0(prop_name, "ProtocolVersion") == 0) {
      // what is this supposed to be?
      return g_variant_new_int32(1);
    } else {
      // randomness
      return g_variant_new_boolean(false);
    }
  } 
  const GDBusInterfaceVTable watcher::m_vtable = {
    handle_method_call,
    handle_get_property,
    NULL
  };

  void watcher::on_bus_acquired(GDBusConnection *c, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    This->m_log.info("tray-watcher: acquired connection to DBus"); 
    guint reg_id = g_dbus_connection_register_object(c, WATCHER_PATH, This->m_intro_data->interfaces[0], &m_vtable, This, nullptr, nullptr);
    g_assert(reg_id > 0);
  }

  void watcher::on_name_acquired(GDBusConnection *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    This->m_log.info("tray-watcher: name successfully acquired");
  }

  void watcher::on_name_lost(GDBusConnection *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    This->m_log.warn("tray-watcher: could not register name, StatusNotifierWatcher might already exist");
    // self-destruct
  }


  host::host() {
    // establish icon theme, main loop?
    // is this needed if watcher and host are combined?
    auto name = "org.freedesktop.StatusNotifierHost-" + to_string(getpid());
    
  }
}

POLYBAR_NS_END
