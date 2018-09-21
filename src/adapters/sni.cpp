//#include "sni-generated.h"
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
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
  /**
   * Construct watcher object
   */
  // run main loop on separate thread, notify hosts for each event?
  watcher::watcher(const logger& logger, queue& queue) : m_log(logger), m_queue(queue) {
    m_intro_data = g_dbus_node_info_new_for_xml(XML_DATA, nullptr);
    g_assert(m_intro_data != nullptr);

    m_mainloop = g_main_loop_new(nullptr, FALSE);
    m_id = g_bus_own_name(G_BUS_TYPE_SESSION, WATCHER_NAME, G_BUS_NAME_OWNER_FLAGS_REPLACE,
        on_bus_acquired, on_name_acquired, on_name_lost, this, nullptr);
    g_main_loop_run(m_mainloop);
    m_log.warn("hello");
  }

  /**
   * Deconstruct watcher
   */
  watcher::~watcher() {
    g_dbus_node_info_unref(m_intro_data);
    if (g_main_loop_is_running(m_mainloop)) {
      g_bus_unown_name(m_id);
      g_main_loop_quit(m_mainloop);
    }
    g_main_loop_unref(m_mainloop);
  }

  /**
   * Get all StatusNotifierItems
   */
  const vector<string>& watcher::get_items() {
    return m_items;
  }

  /**
   * Callback when new item wants to be registered
   */
  void watcher::item_appeared_handler(GDBusConnection *, const gchar *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    auto item = This->tmp_param;
    This->m_items.emplace_back(item);
    This->m_log.info("tray-watcher: new item %s, managing %d items", item, This->m_items.size());
    This->m_queue.enqueue(evtype::WATCHER_NEW);
  }

  /**
   * Callback when item has disappeared
   */
  void watcher::item_vanished_handler(GDBusConnection *c, const gchar *, gpointer userdata) {
   watcher *This = static_cast<watcher *>(userdata);
   auto item = This->tmp_param;
   This->m_items.erase(std::remove(This->m_items.begin(), This->m_items.end(), item), This->m_items.end());
   This->m_log.info("tray-watcher: removing item %s, managing %d items", item, This->m_items.size());
   This->m_queue.enqueue(evtype::WATCHER_DELETE);
   g_dbus_connection_emit_signal(c, nullptr, WATCHER_PATH, WATCHER_NAME, "StatusNotifierItemUnregistered",
       g_variant_new("(s)", item.c_str()), nullptr);
  }

  /**
   * Callback when new host wants to be registered
   */
  void watcher::host_appeared_handler(GDBusConnection *, const gchar *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    auto host = This->tmp_param;
    This->m_hosts.emplace_back(host);
    This->m_log.info("tray-watcher: new host %s, managing %d hosts", host, This->m_hosts.size());
  }

  /**
   * Callback when host has disappeared
   */
  void watcher::host_vanished_handler(GDBusConnection *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    auto host = This->tmp_param;
    This->m_hosts.erase(std::remove(This->m_hosts.begin(), This->m_hosts.end(), host), This->m_hosts.end());
    This->m_log.info("tray-watcher: removing host %s, managing %d hosts", host, This->m_hosts.size());
  }

  /**
   * Callback when a StatusNotifierWatcher method has been called
   */
  void watcher::handle_method_call(GDBusConnection *c, const gchar *sender, const gchar *,
      const gchar *, const gchar *method_name, GVariant *param, GDBusMethodInvocation *invoc,
      gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    // check whether xembedsniproxy is being used
    bool proxy = false;
    const gchar *tmp{nullptr};
    string service{};
    g_variant_get(param, "(&s)", &tmp);
    string param_str{tmp};
    //g_variant_get(param, "(&s)", *(param_str.c_str()));
    // write directly to char buffer of string
    //g_variant_get(param, "(&s)", &param_str.front());
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
      if (std::find(This->m_items.begin(), This->m_items.end(), service) != This->m_items.end()) {
        return;
      }

      g_dbus_method_invocation_return_value(invoc, nullptr);
      g_dbus_connection_emit_signal(c, nullptr, WATCHER_PATH, WATCHER_NAME, "StatusNotifierItemRegistered",
          g_variant_new("(s)", service.c_str()), nullptr);
      // TODO use some other method for storing service (what if another item appears in between?)
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

  /**
   * Callback when a property has been requested
   */
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

  /**
   * Interface table for property/method handlers
   */
  const GDBusInterfaceVTable watcher::m_vtable = {
    handle_method_call,
    handle_get_property,
    NULL
  };

  /**
   * Callback when a DBus connection has been acquired
   */
  void watcher::on_bus_acquired(GDBusConnection *c, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    This->m_log.info("tray-watcher: acquired connection to DBus");
    guint reg_id = g_dbus_connection_register_object(c, WATCHER_PATH, This->m_intro_data->interfaces[0], &m_vtable, This, nullptr, nullptr);
    g_assert(reg_id > 0);
  }

  /**
   * Callback when a name has been acquired
   */
  void watcher::on_name_acquired(GDBusConnection *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    This->m_log.info("tray-watcher: name successfully acquired");
    This->m_queue.enqueue(evtype::WATCHER_ACQUIRED);
  }

  /**
   * Callback when a name could not be acquired or has been lost
   */
  void watcher::on_name_lost(GDBusConnection *, const gchar *, gpointer userdata) {
    watcher *This = static_cast<watcher *>(userdata);
    This->m_log.warn("tray-watcher: could not register name, StatusNotifierWatcher might already exist");
    This->m_queue.enqueue(evtype::WATCHER_LOST);
    // self-destruct
    g_bus_unown_name(This->m_id);
    g_main_loop_quit(This->m_mainloop);
    This->m_log.warn("tray-watcher: stopped watcher");
  }

  /**
   * Construct host object
   */
  host::host(const logger& logger, bool watcher_exists, queue& queue) : m_log(logger), m_queue(queue) {
    // need to init gtk so we can get the icon theme
    if (!gtk_init_check(0, nullptr)) {
      throw host_error("failed to inialize gtk");
    }
    m_theme = gtk_icon_theme_get_default();
    // only run host if watcher already exists (and isn't ours)
    if (watcher_exists) {
      m_log.warn("greetings");
    }
    m_hostname = "org.freedesktop.StatusNotifierHost-" + to_string(getpid());
    m_mainloop = g_main_loop_new(nullptr, false);
    m_id = g_bus_own_name(G_BUS_TYPE_SESSION, m_hostname.c_str(), G_BUS_NAME_OWNER_FLAGS_NONE, nullptr, on_name_acquired, on_name_lost,
        this, nullptr);
    m_thread = std::thread([&] {
      g_main_loop_run(m_mainloop);
    });
    m_thread.detach();
  }

  /**
   * Deconstruct host
   */
  host::~host() {
    if (m_mainloop != nullptr) {
      g_bus_unown_name(m_id);
      g_main_loop_unref(m_mainloop);
    }
  }

  vector<unsigned char> host::pixbuf_to_char(GdkPixbuf *buf) {
    auto pix = gdk_pixbuf_read_pixels(buf);
    auto len = gdk_pixbuf_get_byte_length(buf);
    return vector<unsigned char>(pix, pix + len);
  }

  /**
   * Get item icons from list
   */
  vector<vector<unsigned char>> host::get_items() {
    //std::lock_guard<std::mutex> guard(m_mutex);
    vector<vector<unsigned char>> items{};

    // awkward loading from gdk to uint8 array, maybe change image storage to gdkpixbuf instead
    for (auto&& i : m_items) {
      GdkPixbuf *buf{nullptr};
      // TODO implement GError handling
      if (!i.theme_path.empty()) {
        buf = gtk_icon_theme_load_icon(m_theme, i.icon_name.c_str(), 24, GTK_ICON_LOOKUP_USE_BUILTIN, nullptr);
        if (!buf) {
          m_log.info("icon %s doesn't exist in current theme, adding %s to search paths", i.icon_name, i.theme_path);
          gtk_icon_theme_append_search_path(m_theme, i.theme_path.c_str());
          buf = gtk_icon_theme_load_icon(m_theme, i.icon_name.c_str(), 24, GTK_ICON_LOOKUP_USE_BUILTIN, nullptr);
        }
      } else {
        // gtk_icon_theme_load_surface is nice, but we currently don't pass surfaces
        buf = gtk_icon_theme_load_icon(m_theme, i.icon_name.c_str(), 24, GTK_ICON_LOOKUP_USE_BUILTIN, nullptr);
      }

      if (!buf) {
        m_log.warn("Could not load image %s for %s", i.icon_name, i.id);
        continue;
      }
      items.emplace_back(pixbuf_to_char(buf));
      g_object_unref(buf);
    }
    return items;
  }

  // cached or non-cached?
  // TODO neater way of setting width and height (passing data is messy?)
  /**
   * Retrieve cached pixmap (returned as string)
   */
  string host::get_prop_pixmap(GDBusProxy *p, const char *prop, item_data& data) {
    string retstr = ""s;
    GVariant *val = g_dbus_proxy_get_cached_property(p, prop);
    if (val != nullptr) {
      GVariantIter *iter;
      g_variant_get(val, "a(iiay)", &iter);
      char tmp;
      while(g_variant_iter_loop(iter, "(iiay)", &(data.icon_width), &(data.icon_height), &tmp)) {
        retstr.push_back(tmp);
      }
      g_variant_iter_free(iter);
      g_variant_unref(val);
    }
    return retstr;
  }

  /**
   * Retrieve property string (not from cache)
   */
  string host::get_prop_string(GDBusProxy *p, const char *prop) {
    string retstr = string{""};
    GVariant *val = g_dbus_proxy_call_sync(p, "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", "org.kde.StatusNotifierItem", prop), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr);
    if (val != nullptr) {
      GVariant *var = nullptr;
      g_variant_get(val, "(v)", &var);
      if (var != nullptr) {
        retstr = string{g_variant_get_string(var, nullptr)};
        g_variant_unref(var);
      }
      g_variant_unref(val);
    }
    return retstr;
  }

  /**
   * Initialize all data for item
   */
  item_data host::init_item_data(const string& name, const string& path) {
    GDBusProxy *p = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr, name.c_str(), path.c_str(),
       "org.kde.StatusNotifierItem", nullptr, nullptr);
    item_data data;
    //g_signal_connect(p, "g-signal", G_CALLBACK(on_item_sig_changed), &data);
    g_signal_connect(p, "g-signal", G_CALLBACK(on_item_sig_changed), this);
    data.proxy = p;
    data.dbus_name = string{name};
    data.category = get_prop_string(p, "Category");
    data.id = get_prop_string(p, "Id");
    data.title = get_prop_string(p, "Status");
    // TODO: set icon path here, or before drawing?
    data.icon_name = get_prop_string(p, "IconName");
    data.theme_path = get_prop_string(p, "IconThemePath");
    data.icon_pixmap = get_prop_pixmap(p, "IconPixmap", data);
    //data->overlay_name = get_prop_string(p, "OverlayIconName");
    //data->att_name = get_prop_string(p, "AttentionIconName");
    //data->movie_name = get_prop_string(p, "AttentionMovieName");
    // tooltip?
    return data;
  }

  /**
   * Callback when watcher has changed
   */
  void host::on_watch_sig_changed(GDBusProxy *, gchar *, gchar *signal, GVariant *param, gpointer userdata) {
    host *This = static_cast<host *>(userdata);
    //std::lock_guard<std::mutex> guard(This->m_mutex);
    string item, name, path;
    // gchars to string?
    if (g_strcmp0(signal, "StatusNotifierItemRegistered") == 0) {
      const gchar *tmp{nullptr};
      g_variant_get(param, "(&s)", &tmp);
      string item{tmp};
      name = string{item, 0, item.find('/')};
      path = string{item, item.find('/')};
      This->m_log.info("tray-host: item %s has been registered", item);
      This->m_items.emplace_back(This->init_item_data(name, path));
    } else if (g_strcmp0(signal, "StatusNotifierItemUnregistered") == 0) {
      const gchar *tmp{nullptr};
      g_variant_get(param, "(&s)", &tmp);
      string item{tmp};
      name = string{item, 0, item.find('/')};
      path = string{item, item.find('/')};
      This->m_log.info("tray-host: item %s has been unregistered", item);
      for (auto it = This->m_items.begin(); it != This->m_items.end(); it++) {
        if (it->dbus_name.compare(name) == 0) {
          This->m_items.erase(it);
          break;
        }
      }
    }
    // signal to redraw tray
    This->m_queue.enqueue(evtype::HOST_ITEM);
  }

  /**
   * Callback when an item has changed
   */
  void host::on_item_sig_changed(GDBusProxy *p, gchar *sender, gchar *signal, GVariant *, gpointer userdata) {
    //item_data *item = static_cast<item_data *>(userdata);
    host *This = static_cast<host *>(userdata);
    item_data *item{nullptr};
    for (auto it = This->m_items.begin(); it != This->m_items.end(); it++) {
      if (it->dbus_name.compare(sender) == 0) {
        item = &*it;
        break;
      }
    }
    if (item != nullptr) {
      if (g_strcmp0(signal, "NewTitle") == 0) {
        item->title = This->get_prop_string(p, "Title");
      } else if (g_strcmp0(signal, "NewIcon") == 0) {
        item->icon_name = This->get_prop_string(p, "IconName");
        //if ((item->icon_name = get_prop_string(p, "IconName")) != nullptr)
        //  ensure_icon_path(p, data->icon_name, data->icon_path);
        item->icon_pixmap = This->get_prop_pixmap(p, "IconPixmap", *item);
      } else if (g_strcmp0(signal, "NewStatus") == 0) {
        item->status = This->get_prop_string(p, "Status");
      }
    }
    // signal to redraw tray
    This->m_queue.enqueue(evtype::HOST_CHANGE);
  }

  /**
   * Callback when watcher has appeared
   */
  void host::watcher_appeared_handler(GDBusConnection *c, const gchar *, const gchar *, gpointer userdata) {
    host *This = static_cast<host *>(userdata);
    g_dbus_connection_call_sync(c, watcher::WATCHER_NAME, watcher::WATCHER_PATH, watcher::WATCHER_NAME, "RegisterStatusNotifierHost",
        g_variant_new("(s)", This->m_hostname.c_str()), nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr);
    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr, watcher::WATCHER_NAME,
        watcher::WATCHER_PATH, watcher::WATCHER_NAME, nullptr, nullptr);

    g_signal_connect(proxy, "g-signal", G_CALLBACK(on_watch_sig_changed), userdata);

    GVariant *items = g_dbus_proxy_get_cached_property(proxy, "RegisteredStatusNotifierItems"), *content;
    if (items != nullptr) {
      //std::lock_guard<std::mutex> guard(This->m_mutex);
      This->m_log.warn("has stuff inside");
      GVariantIter *it = g_variant_iter_new(items);
      //for (it = g_variant_iter_new(items); content != nullptr; content = g_variant_iter_next_value(it)) {
      while ((content = g_variant_iter_next_value(it))) {
        string tmp = string{g_variant_get_string(content, nullptr)};
        // name until first '/'
        string it_name = string{tmp, 0, tmp.find('/')};
        // path after (including) first '/'
        string it_path = string{tmp, tmp.find('/')};
        This->m_items.emplace_back(This->init_item_data(it_name, it_path));
        This->m_log.info("tray-host: item %s/%s has been registered", it_name, it_path);
      }
      g_variant_iter_free(it);
      g_variant_unref(items);
    }
    // signal to redraw tray
    This->m_queue.enqueue(evtype::HOST_INIT);
  }

  /**
   * Callback when watcher has disappeared
   */
  void host::watcher_vanished_handler(GDBusConnection *, const gchar *, gpointer userdata) {
    host *This = static_cast<host *>(userdata);
    This->m_log.warn("tray-host: watcher has disappeared");
    // TODO create a new watcher to replace the old one
    // also make sure that if multiple hosts exist, they don't create multiple watchers (no race condition)?
  }

  /**
   * Callback when a name has been acquired
   */
  void host::on_name_acquired(GDBusConnection *, const gchar *, gpointer userdata) {
    host *This = static_cast<host *>(userdata);
    This->m_log.info("tray-host: name successfully acquired");
    g_bus_watch_name(G_BUS_TYPE_SESSION, watcher::WATCHER_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
        watcher_appeared_handler, watcher_vanished_handler, userdata, nullptr);
    This->m_queue.enqueue(evtype::HOST_ACQUIRED);
  }

  /**
   * Callback when a name could not be acquired or has been lost
   */
  void host::on_name_lost(GDBusConnection *, const gchar *, gpointer userdata) {
    host *This = static_cast<host *>(userdata);
    This->m_log.err("tray-host: could not register name");
    This->m_queue.enqueue(evtype::HOST_LOST);
  }
}

POLYBAR_NS_END
