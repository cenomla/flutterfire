#include "include/firebase_core/fl_firebase_core_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <firebase/app.h>

// Flutter method channel name
const char kChannelName[] = "plugins.flutter.io/firebase_core";
const char kBadArgumentsError[] = "Bad Arguments";

// Firebase method names
const char kMethodCoreInitializeApp[] = "Firebase#initializeApp";
const char kMethodCoreInitializeCore[] = "Firebase#initializeCore";

// FirebaseApp method names
const char kMethodAppDelete[] = "FirebaseApp#delete";
const char kMethodAppSetAutomaticDataCollectionEnabled[] = "FirebaseApp#setAutomaticDataCollectionEnabled";
const char kMethodAppSetAutomaticResourceManagementEnabled[] = "FirebaseApp#setAutomaticResourceManagementEnabled";

// Method call argument keys.
const char kName[] = "name";
const char kAppName[] = "appName";
const char kOptions[] = "options";
const char kFirebaseDefaultAppName[] = "[DEFAULT]";
//const char kEnabled[] = "enabled";
//const char kPluginConstants[] = "pluginConstants";
//const char kIsAutomaticDataCollectionEnabled[] = "isAutomaticDataCollectionEnabled";
const char kFirebaseOptionsApiKey[] = "apiKey";
const char kFirebaseOptionsAppId[] = "appId";
const char kFirebaseOptionsMessagingSenderId[] = "messagingSenderId";
const char kFirebaseOptionsProjectId[] = "projectId";
const char kFirebaseOptionsDatabaseUrl[] = "databaseURL";
const char kFirebaseOptionsStorageBucket[] = "storageBucket";
const char kFirebaseOptionsTrackingId[] = "trackingId";
//const char kFirebaseOptionsDeepLinkURLScheme[] = "deepLinkURLScheme";
//const char kFirebaseOptionsAndroidClientId[] = "androidClientId";
//const char kFirebaseOptionsIosBundleId[] = "iosBundleId";
//const char kFirebaseOptionsIosClientId[] = "iosClientId";
//const char kFirebaseOptionsAppGroupId[] = "appGroupId";

struct _FlFirebaseCorePlugin {
  // Parent instance to allow for this stuct to be a GObject
  GObject parent_instance;

  // Flutter plugin registrar
  FlPluginRegistrar* registrar;

  // Connection to Flutter engine.
  FlMethodChannel* channel;

  bool _coreInitialized;
};

G_DEFINE_TYPE(FlFirebaseCorePlugin, fl_firebase_core_plugin, g_object_get_type())

static char const* get_firebase_app_name(char const *name) {
	if (strcmp(name, firebase::kDefaultAppName) == 0) {
		return kFirebaseDefaultAppName;
	}
	return name;
}

static FlValue* firebase_app_to_map(firebase::App *app, bool isDefault = false) {
	FlValue *appMap = fl_value_new_map();
	FlValue *optionsMap = fl_value_new_map();

	firebase::AppOptions const& options = app->options();

	fl_value_set_take(optionsMap, fl_value_new_string(kFirebaseOptionsApiKey), fl_value_new_string(options.api_key()));
	fl_value_set_take(optionsMap, fl_value_new_string(kFirebaseOptionsAppId), fl_value_new_string(options.app_id()));
	if (options.messaging_sender_id())
		fl_value_set_take(optionsMap,
				fl_value_new_string(kFirebaseOptionsMessagingSenderId), fl_value_new_string(options.messaging_sender_id()));
	if (options.project_id())
		fl_value_set_take(optionsMap,
				fl_value_new_string(kFirebaseOptionsProjectId), fl_value_new_string(options.project_id()));
	if (options.database_url())
		fl_value_set_take(optionsMap,
				fl_value_new_string(kFirebaseOptionsDatabaseUrl), fl_value_new_string(options.database_url()));
	if (options.storage_bucket())
		fl_value_set_take(optionsMap,
				fl_value_new_string(kFirebaseOptionsStorageBucket), fl_value_new_string(options.storage_bucket()));
	if (options.ga_tracking_id())
		fl_value_set_take(optionsMap,
				fl_value_new_string(kFirebaseOptionsTrackingId), fl_value_new_string(options.ga_tracking_id()));

	fl_value_set_take(appMap, fl_value_new_string(kName), fl_value_new_string(get_firebase_app_name(app->name())));
	fl_value_set_take(appMap, fl_value_new_string(kOptions), optionsMap);

	return appMap;
}

static FlMethodResponse* initialize_firebase_app(FlValue *args) {
	// This function takes a map with the app name and options

	if (fl_value_get_type(args) != FL_VALUE_TYPE_MAP
			|| fl_value_get_length(args) != 2) {
		return FL_METHOD_RESPONSE(fl_method_error_response_new(kBadArgumentsError, "Expected map with 2 elements", nullptr));
	}

	// Get the name and options from the arguments
	FlValue *appName = fl_value_lookup_string(args, kAppName);
	FlValue *options = fl_value_lookup_string(args, kOptions);

	firebase::AppOptions appOptions;
	appOptions.set_api_key(fl_value_get_string(fl_value_lookup_string(options, kFirebaseOptionsApiKey)));
	appOptions.set_app_id(fl_value_get_string(fl_value_lookup_string(options, kFirebaseOptionsAppId)));

	FlValue *optionMessagingSenderId = fl_value_lookup_string(options, kFirebaseOptionsMessagingSenderId);
	if (fl_value_get_type(optionMessagingSenderId) != FL_VALUE_TYPE_NULL)
		appOptions.set_messaging_sender_id(fl_value_get_string(optionMessagingSenderId));

	FlValue *optionProjectId = fl_value_lookup_string(options, kFirebaseOptionsProjectId);
	if (fl_value_get_type(optionProjectId) != FL_VALUE_TYPE_NULL)
		appOptions.set_project_id(fl_value_get_string(optionProjectId));

	FlValue *optionDatabaseUrl = fl_value_lookup_string(options, kFirebaseOptionsDatabaseUrl);
	if (fl_value_get_type(optionDatabaseUrl) != FL_VALUE_TYPE_NULL)
		appOptions.set_database_url(fl_value_get_string(optionDatabaseUrl));

	FlValue *optionStorageBucket = fl_value_lookup_string(options, kFirebaseOptionsStorageBucket);
	if (fl_value_get_type(optionStorageBucket) != FL_VALUE_TYPE_NULL)
		appOptions.set_storage_bucket(fl_value_get_string(optionStorageBucket));

	FlValue *optionTrackingId = fl_value_lookup_string(options, kFirebaseOptionsTrackingId);
	if (fl_value_get_type(optionTrackingId) != FL_VALUE_TYPE_NULL)
		appOptions.set_ga_tracking_id(fl_value_get_string(optionTrackingId));

	firebase::App *app = firebase::App::Create(appOptions, fl_value_get_string(appName));

    return FL_METHOD_RESPONSE(fl_method_success_response_new(firebase_app_to_map(app)));
}

static FlMethodResponse* initialize_firebase_core(_FlFirebaseCorePlugin *self) {
	// This function returns a list of map objects which define a firebase app, the entries into the map are name and options
	if (!self->_coreInitialized) {
		// Initialize the default app
		firebase::App *app = firebase::App::Create();
		if (!app) {
			g_warning("Failed to create default app");
		}

		self->_coreInitialized = true;
	}

	FlValue *apps = fl_value_new_list();

	// Add the default instance to the list of existing firebase apps
	firebase::App *defaultInst = firebase::App::GetInstance();
	if (defaultInst)
		fl_value_append_take(apps, firebase_app_to_map(defaultInst));

	return FL_METHOD_RESPONSE(fl_method_success_response_new(apps));
}

static FlMethodResponse* delete_firebase_app() {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* set_firebase_automatic_data_collection_enabled() {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* set_firebase_automatic_resource_management_enabled() {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

// Called when a method call is received from Flutter.
static void method_call_cb(FlMethodChannel* channel, FlMethodCall* method_call,
                           gpointer user_data) {
  FlFirebaseCorePlugin* self = FL_FIREBASE_CORE_PLUGIN(user_data);

  const gchar* method = fl_method_call_get_name(method_call);
  FlValue* args = fl_method_call_get_args(method_call);

  g_autoptr(FlMethodResponse) response = nullptr;
  if (strcmp(method, kMethodCoreInitializeApp) == 0) {
    response = initialize_firebase_app(args);
  } else if (strcmp(method, kMethodCoreInitializeCore) == 0) {
    response = initialize_firebase_core(self);
  } else if (strcmp(method, kMethodAppDelete) == 0) {
    response = delete_firebase_app();
  } else if (strcmp(method, kMethodAppSetAutomaticDataCollectionEnabled) == 0) {
    response = set_firebase_automatic_data_collection_enabled();
  } else if (strcmp(method, kMethodAppSetAutomaticResourceManagementEnabled) == 0) {
    response = set_firebase_automatic_resource_management_enabled();
  } else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  g_autoptr(GError) error = nullptr;
  if (!fl_method_call_respond(method_call, response, &error))
    g_warning("Failed to send method call response: %s", error->message);
}

static void fl_firebase_core_plugin_dispose(GObject* object) {
  FlFirebaseCorePlugin* self = FL_FIREBASE_CORE_PLUGIN(object);

  g_clear_object(&self->registrar);
  g_clear_object(&self->channel);

  G_OBJECT_CLASS(fl_firebase_core_plugin_parent_class)->dispose(object);
}

static void fl_firebase_core_plugin_class_init(FlFirebaseCorePluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = fl_firebase_core_plugin_dispose;
}

static void fl_firebase_core_plugin_init(FlFirebaseCorePlugin* self) {
	// This function gets called as apart of the gobject initialization, do plugin init here
	self->_coreInitialized = false;
}

FlFirebaseCorePlugin* fl_firebase_core_plugin_new(FlPluginRegistrar* registrar) {
  FlFirebaseCorePlugin* self = FL_FIREBASE_CORE_PLUGIN(
      g_object_new(fl_firebase_core_plugin_get_type(), nullptr));

  self->registrar = FL_PLUGIN_REGISTRAR(g_object_ref(registrar));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  self->channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            kChannelName, FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(self->channel, method_call_cb,
                                            g_object_ref(self), g_object_unref);

  return self;
}

void fl_firebase_core_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  FlFirebaseCorePlugin* plugin = fl_firebase_core_plugin_new(registrar);
  g_object_unref(plugin);
}

