#include "include/clipboard_monitor/clipboard_monitor_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>

#include <cstring>

// https://stackoverflow.com/questions/8755471/x11-wait-for-and-get-clipboard-text
// https://stackoverflow.com/questions/27378318/c-get-string-from-clipboard-on-linux/44992938#44992938

#define CLIPBOARD_MONITOR_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), clipboard_monitor_plugin_get_type(), \
                              ClipboardMonitorPlugin))

struct _ClipboardMonitorPlugin {
  GObject parent_instance;
};

G_DEFINE_TYPE(ClipboardMonitorPlugin, clipboard_monitor_plugin, g_object_get_type())

// Called when a method call is received from Flutter.
static void clipboard_monitor_plugin_handle_method_call(
    ClipboardMonitorPlugin* self,
    FlMethodCall* method_call) {
  g_autoptr(FlMethodResponse) response = nullptr;

  const gchar* method = fl_method_call_get_name(method_call);

  if (strcmp(method, "getPlatformVersion") == 0) {
    struct utsname uname_data = {};
    uname(&uname_data);
    g_autofree gchar *version = g_strdup_printf("Linux %s", uname_data.version);
    g_autoptr(FlValue) result = fl_value_new_string(version);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  } else if (strcmp(method, "monitorClipboard") == 0) {
    printf("monitorClipboard\n");
  } else if (strcmp(method, "stopMonitoringClipboard") == 0) {
    printf("stopMonitoringClipboard\n");
  } else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  fl_method_call_respond(method_call, response, nullptr);
}

static void clipboard_monitor_plugin_dispose(GObject* object) {
  G_OBJECT_CLASS(clipboard_monitor_plugin_parent_class)->dispose(object);
}

static void clipboard_monitor_plugin_class_init(ClipboardMonitorPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = clipboard_monitor_plugin_dispose;
}

static void clipboard_monitor_plugin_init(ClipboardMonitorPlugin* self) {}

static void method_call_cb(FlMethodChannel* channel, FlMethodCall* method_call,
                           gpointer user_data) {
  ClipboardMonitorPlugin* plugin = CLIPBOARD_MONITOR_PLUGIN(user_data);
  clipboard_monitor_plugin_handle_method_call(plugin, method_call);
}

void clipboard_monitor_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  ClipboardMonitorPlugin* plugin = CLIPBOARD_MONITOR_PLUGIN(
      g_object_new(clipboard_monitor_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  g_autoptr(FlMethodChannel) channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            "clipboard_monitor",
                            FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(channel, method_call_cb,
                                            g_object_ref(plugin),
                                            g_object_unref);

  g_object_unref(plugin);
}
