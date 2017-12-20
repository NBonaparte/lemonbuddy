#pragma once

//#include "sni-generated.h"
#include <gio/gio.h>
#include "common.hpp"
#include "components/logger.hpp"

POLYBAR_NS

namespace sni {
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
     host();
   };
}

POLYBAR_NS_END
