#pragma once

//#include "sni-generated.h"
#include <gio/gio.h>
#include "common.hpp"
#include "errors.hpp"

POLYBAR_NS
class logger;

DEFINE_ERROR(watcher_error);
DEFINE_ERROR(host_error);

namespace sni {

  struct item_data {
    GDBusProxy *proxy,
    string dbus_name,
    string category,
    string id,
    string title,
    string status,
    uint32_t win_id,
    string icon_name,
    string icon_path,
    string theme_path, // needed?

    int icon_width,
    int icon_height,
    string icon_pixmap, // argb32 encoded? base64? also need width/height

    string overlay_name,
    string attention_name,

    int attention_width,
    int attention_height,
    string attention_pixmap, // argb32 encoded?

    string movie_name,
    // TODO: tooltip icon name, icon pixmap, title, description
    bool ismenu,
    GVariant *obj_path,
  };

  class watcher {
   public:
    watcher(const logger& logger);
    ~watcher();
   private:
    static void item_appeared_handler(GDBusConnection *c, const gchar *name, const gchar *owner, gpointer user_data);
    static void item_vanished_handler(GDBusConnection *c, const gchar *name, gpointer user_data);
    static void host_appeared_handler(GDBusConnection *c, const gchar *name, const gchar *owner, gpointer user_data);
    static void host_vanished_handler(GDBusConnection *c, const gchar *name, gpointer user_data);

    static void handle_method_call(GDBusConnection *c, const gchar *sender, const gchar *obj_path,
        const gchar *int_name, const gchar *method_name, GVariant *param, GDBusMethodInvocation *invoc,
        gpointer user_data);
    static GVariant *handle_get_property(GDBusConnection *c, const gchar *sender, const gchar *obj_path,
        const gchar *int_name, const gchar *prop_name, GError **err, gpointer user_data);
    static void on_bus_acquired(GDBusConnection *c, const gchar *name, gpointer user_data);
    static void on_name_acquired(GDBusConnection *c, const gchar *name, gpointer user_data);
    static void on_name_lost(GDBusConnection *c, const gchar *name, gpointer user_data);
    static constexpr const char* WATCHER_NAME{"org.kde.StatusNotifierWatcher"};
    static constexpr const char* WATCHER_PATH{"/StatusNotifierWatcher"};
    static constexpr const char* XML_DATA = 
       "<!DOCTYPE node PUBLIC '-//freedesktop//DTD D-BUS Object Introspection 1.0//EN' 'http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd'>"
       "<node>"
       "	<interface name='org.kde.StatusNotifierWatcher'>"
       "		<method name='RegisterStatusNotifierItem'>"
       "			<arg name='service' type='s' direction='in'/>"
       "		</method>"
       "		<method name='RegisterStatusNotifierHost'>"
       "			<arg name='service' type='s' direction='in'/>"
       "		</method>"
       "		<property name='RegisteredStatusNotifierItems' type='as' access='read'>"
       "			<annotation name='org.qtproject.QtDBus.QtTypeName.Out0' value='QStringList'/>"
       "		</property>"
       "		<property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
       "		<property name='ProtocolVersion' type='i' access='read'/>"
       "		<signal name='StatusNotifierItemRegistered'>"
       "			<arg type='s'/>"
       "		</signal>"
       "		<signal name='StatusNotifierItemUnregistered'>"
       "			<arg type='s'/>"
       "		</signal>"
       "		<signal name='StatusNotifierHostRegistered'>"
       "		</signal>"
       "		<signal name='StatusNotifierHostUnregistered'>"
       "		</signal>"
       "	</interface>"
       "</node>";
    // TODO: replace xml with gdbus-codegen generated code
    GDBusNodeInfo *m_intro_data;
    static const GDBusInterfaceVTable m_vtable;

    GMainLoop *m_mainloop;
    guint m_id;
    vector<std::string> m_hosts{};
    vector<std::string> m_items{};
    string tmp_param;

    const logger& m_log;
  };

  class host {
   public:
    host(const logger& logger);
    ~host();
   private:
    vector<item_data> m_items{};
    string m_theme;
    const logger& m_log;
  };
}

POLYBAR_NS_END
