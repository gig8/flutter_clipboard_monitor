//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <clipboard_monitor/clipboard_monitor_plugin.h>

void fl_register_plugins(FlPluginRegistry* registry) {
  g_autoptr(FlPluginRegistrar) clipboard_monitor_registrar =
      fl_plugin_registry_get_registrar_for_plugin(registry, "ClipboardMonitorPlugin");
  clipboard_monitor_plugin_register_with_registrar(clipboard_monitor_registrar);
}
