#ifndef PLUGINS_FIREBASE_CORE_LINUX_FIREBASE_CORE_PLUGIN_H_
#define PLUGINS_FIREBASE_CORE_LINUX_FIREBASE_CORE_PLUGIN_H_

// A plugin to bind to the firebase c++ sdk

#include <flutter_linux/flutter_linux.h>

G_BEGIN_DECLS

#ifdef FLUTTER_PLUGIN_IMPL
#define FLUTTER_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define FLUTTER_PLUGIN_EXPORT
#endif

G_DECLARE_FINAL_TYPE(FlFirebaseCorePlugin, fl_firebase_core_plugin, FL,
                     FIREBASE_CORE_PLUGIN, GObject)

FLUTTER_PLUGIN_EXPORT FlFirebaseCorePlugin* fl_firebase_core_plugin_new(
    FlPluginRegistrar* registrar);

FLUTTER_PLUGIN_EXPORT void fl_firebase_core_plugin_register_with_registrar(
    FlPluginRegistrar* registrar);

G_END_DECLS

#endif  // PLUGINS_WINDOW_SIZE_LINUX_WINDOW_SIZE_PLUGIN_H_
