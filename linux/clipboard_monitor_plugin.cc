#include "include/clipboard_monitor/clipboard_monitor_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>

#include <limits.h>
#include <X11/Xlib.h>
#include <thread>
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <chrono>
#include <cassert>

#include <X11/extensions/Xfixes.h>

using namespace std;

// https://stackoverflow.com/questions/27378318/c-get-string-from-clipboard-on-linux/44992938#44992938

#define CLIPBOARD_MONITOR_PLUGIN(obj)                                     \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), clipboard_monitor_plugin_get_type(), \
                              ClipboardMonitorPlugin))

struct _ClipboardMonitorPlugin
{
  GObject parent_instance;
};

G_DEFINE_TYPE(ClipboardMonitorPlugin, clipboard_monitor_plugin, g_object_get_type())

// https://stackoverflow.com/questions/8755471/x11-wait-for-and-get-clipboard-text
// also per https://github.com/jschwartzenberg/kicker/blob/master/klipper/clipboardpoll.cpp no xfixes means polling
// PRIMARY (highlight) CLIPBOARD (Ctrl-C)

static Display *display;
static Window window;
static FlMethodChannel *channel = NULL;
bool stayAlive;

void watchSelection(const char *bufname);
bool printSelection(const char *bufname, const char *fmtname);

void watchSelection(const char *bufname)
{
  int event_base, error_base;
  XEvent event;
  Atom bufid = XInternAtom(display, bufname, False);

  assert(XFixesQueryExtension(display, &event_base, &error_base));
  XFixesSelectSelectionInput(display, DefaultRootWindow(display), bufid, XFixesSetSelectionOwnerNotifyMask);

  while (stayAlive)
  {
    XNextEvent(display, &event);

    if (event.type == event_base + XFixesSelectionNotify &&
        ((XFixesSelectionNotifyEvent *)&event)->selection == bufid)
    {
      if (!printSelection(bufname, "UTF8_STRING"))
        printSelection(bufname, "STRING");

      fflush(stdout);
    }
  }
}

bool printSelection(const char *bufname, const char *fmtname)
{
  // this method seems promising but freezes the UI
  // GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  // gchar *text = gtk_clipboard_wait_for_text(clip);
  // auto args = fl_value_new_list();
  // fl_value_append(args, fl_value_new_string(text));
  // fl_method_channel_invoke_method(channel, "cliptext", args,
  //                                   nullptr, nullptr, nullptr);
  // return true;

  char *result;  
  unsigned long ressize, restail;
  int resbits;
  Atom bufid = XInternAtom(display, bufname, False), // PRIMARY or CLIPBOARD
      fmtid = XInternAtom(display, fmtname, False),  // UTF8_STRING or STRING
      propid = XInternAtom(display, "XSEL_DATA", False),
       incrid = XInternAtom(display, "INCR", False);
  XEvent event;

  XConvertSelection(display, bufid, fmtid, propid, window, CurrentTime);

  do
  {
    XNextEvent(display, &event);
  } while (event.type != SelectionNotify || event.xselection.selection != bufid);

  if (event.xselection.property)
  {
    XGetWindowProperty(display, window, propid, 0, LONG_MAX / 4, False, AnyPropertyType,
                       &fmtid, &resbits, &ressize, &restail, (unsigned char **)&result);

    if (fmtid == incrid)
      printf("Buffer is too large and INCR reading is not implemented yet.\n");
    else {
      printf("%.*s\n", (int)ressize, result);
      
      // https://stackoverflow.com/questions/7899119/what-does-s-mean-in-printf

      // how do I call my channel?? how about the thread?
      auto value = fl_value_new_string_sized((gchar *)result, (int)ressize);
      fl_method_channel_invoke_method(channel, "cliptext", value,
                                    nullptr, nullptr, nullptr);
    }

    XFree(result);
    return true;
  }
  return false;
}

// Called when a method call is received from Flutter.
static void clipboard_monitor_plugin_handle_method_call(
    ClipboardMonitorPlugin *self,
    FlMethodCall *method_call)
{
  g_autoptr(FlMethodResponse) response = nullptr;

  const gchar *method = fl_method_call_get_name(method_call);

  if (strcmp(method, "getPlatformVersion") == 0)
  {
    struct utsname uname_data = {};
    uname(&uname_data);
    g_autofree gchar *version = g_strdup_printf("Linux %s", uname_data.version);
    g_autoptr(FlValue) result = fl_value_new_string(version);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  }
  else if (strcmp(method, "monitorClipboard") == 0)
  {
    printf("monitorClipboard\n");

  // g_autoptr(FlValue) args = fl_value_new_list();
  // fl_value_append(args, fl_value_new_string("hello from monitorClipboard"));
  // fl_method_channel_invoke_method(channel, "cliptext", args,
  //                               nullptr, nullptr, nullptr);

    XInitThreads();
    display = XOpenDisplay(NULL);
    unsigned long color = BlackPixel(display, DefaultScreen(display));
    window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, color, color);
    stayAlive = true;

    thread fthread(watchSelection, "CLIPBOARD");
    fthread.detach();

    response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_null()));
  }
  else if (strcmp(method, "stopMonitoringClipboard") == 0)
  {
    printf("stopMonitoringClipboard\n");

    stayAlive = false;
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_null()));
  }
  else
  {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  fl_method_call_respond(method_call, response, nullptr);
}

static void clipboard_monitor_plugin_dispose(GObject *object)
{
  G_OBJECT_CLASS(clipboard_monitor_plugin_parent_class)->dispose(object);
}

static void clipboard_monitor_plugin_class_init(ClipboardMonitorPluginClass *klass)
{
  G_OBJECT_CLASS(klass)->dispose = clipboard_monitor_plugin_dispose;
}

static void clipboard_monitor_plugin_init(ClipboardMonitorPlugin *self) {}

static void method_call_cb(FlMethodChannel *channel, FlMethodCall *method_call,
                           gpointer user_data)
{
  ClipboardMonitorPlugin *plugin = CLIPBOARD_MONITOR_PLUGIN(user_data);
  clipboard_monitor_plugin_handle_method_call(plugin, method_call);
}

void clipboard_monitor_plugin_register_with_registrar(FlPluginRegistrar *registrar)
{
  ClipboardMonitorPlugin *plugin = CLIPBOARD_MONITOR_PLUGIN(
      g_object_new(clipboard_monitor_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();

  channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            "clipboard_monitor",
                            FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(channel, method_call_cb,
                                            g_object_ref(plugin),
                                            g_object_unref);
  g_object_unref(plugin);
}
