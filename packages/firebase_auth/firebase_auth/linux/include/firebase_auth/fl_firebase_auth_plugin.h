#ifndef PLUGINS_FIREBASE_AUTH_LINUX_FIREBASE_AUTH_PLUGIN_H_
#define PLUGINS_FIREBASE_AUTH_LINUX_FIREBASE_AUTH_PLUGIN_H_

// A plugin to bind to the firebase c++ sdk
// NOTE: We use the Fl prefix here instead of FLT cause flutter converts the FLT prefix into f_l_t for all of the c++ file and function names (or maybe glib does that) which is obnoxious

#include <flutter_linux/flutter_linux.h>

G_BEGIN_DECLS

#ifdef FLUTTER_PLUGIN_IMPL
#define FLUTTER_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define FLUTTER_PLUGIN_EXPORT
#endif

G_DECLARE_FINAL_TYPE(FlFirebaseAuthPlugin, fl_firebase_auth_plugin, FL,
                     FIREBASE_AUTH_PLUGIN, GObject)

FLUTTER_PLUGIN_EXPORT FlFirebaseAuthPlugin* fl_firebase_auth_plugin_new(
    FlPluginRegistrar* registrar);

FLUTTER_PLUGIN_EXPORT void fl_firebase_auth_plugin_register_with_registrar(
    FlPluginRegistrar* registrar);

G_END_DECLS

#endif  // PLUGINS_WINDOW_SIZE_LINUX_WINDOW_SIZE_PLUGIN_H_
