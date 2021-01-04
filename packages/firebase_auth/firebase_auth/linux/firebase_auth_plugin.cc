#include "include/firebase_auth/fl_firebase_auth_plugin.h"

#include <firebase/auth/credential.h>
#include <firebase/util.h>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <random>

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <firebase/app.h>
#include <firebase/auth.h>

// Flutter method channel name
const char kChannelName[] = "plugins.flutter.io/firebase_auth";
const char kBadArgumentsError[] = "Bad Arguments";
const char kAuthError[] = "Auth Failed";
const char kFirebaseDefaultAppName[] = "[DEFAULT]";

// Provider type keys
//const char kSignInMethodPassword[] = "password";
//const char kSignInMethodEmailLink[] = "emailLink";
//const char kSignInMethodFacebook[] = "facebook.com";
//const char kSignInMethodGoogle[] = "google.com";
//const char kSignInMethodTwitter[] = "twitter.com";
//const char kSignInMethodGithub[] = "github.com";
//const char kSignInMethodPhone[] = "phone";
//const char kSignInMethodOAuth[] = "oauth";

// Credential argument keys.
//const char kArgumentCredential[] = "credential";
const char kArgumentProviderId[] = "providerId";
const char kArgumentSignInMethod[] = "signInMethod";
//const char kArgumentSecret[] = "secret";
//const char kArgumentIdToken[] = "idToken";
//const char kArgumentAccessToken[] = "accessToken";
//const char kArgumentRawNonce[] = "rawNonce";
const char kArgumentEmail[] = "email";
//const char kArgumentCode[] = "code";
//const char kArgumentNewEmail[] = "newEmail";
//const char *kArgumentEmailLink = kSignInMethodEmailLink;
const char kArgumentToken[] = "token";
//const char kArgumentVerificationId[] = "verificationId";
//const char kArgumentSmsCode[] = "smsCode";
//const char kArgumentActionCodeSettings[] = "actionCodeSettings";

class MyAuthStateListener : public firebase::auth::AuthStateListener {

public:

	MyAuthStateListener(FlFirebaseAuthPlugin *plugin);

	virtual void OnAuthStateChanged(firebase::auth::Auth *auth) override;

private:
	FlFirebaseAuthPlugin *_pluginState = nullptr;
};

class MyIdTokenListener : public firebase::auth::IdTokenListener {

public:

	MyIdTokenListener(FlFirebaseAuthPlugin *plugin);

	virtual void OnIdTokenChanged(firebase::auth::Auth *auth) override;

private:
	FlFirebaseAuthPlugin *_pluginState = nullptr;
};

struct _FlFirebaseAuthPlugin {
  // Parent instance to allow for this stuct to be a GObject
  GObject parent_instance;

  // Flutter plugin registrar
  FlPluginRegistrar* registrar;

  // Connection to Flutter engine.
  FlMethodChannel* channel;

  std::unique_ptr<MyAuthStateListener> authListener;
  std::unique_ptr<MyIdTokenListener> idListener;

  std::unordered_map<uint64_t, firebase::auth::Credential> userCredentials;
  std::unordered_set<std::string> authInitializedApps;
};

G_DEFINE_TYPE(FlFirebaseAuthPlugin, fl_firebase_auth_plugin, g_object_get_type())

static char const* get_internal_app_name(char const *name) {
	if (strcmp(name, kFirebaseDefaultAppName) == 0) {
		return firebase::kDefaultAppName;
	}
	return name;
}

static char const* get_firebase_app_name(char const *name) {
	if (strcmp(name, firebase::kDefaultAppName) == 0) {
		return kFirebaseDefaultAppName;
	}
	return name;
}

static void ensure_auth_initialized(FlFirebaseAuthPlugin *self, char const *appName) {
	if (self->authInitializedApps.find(get_internal_app_name(appName)) != self->authInitializedApps.end()) {
		return;
	}
	// Initialize the default app
	firebase::App *app = firebase::App::GetInstance(get_internal_app_name(appName));
	firebase::ModuleInitializer initializer;
	initializer.Initialize(app, self, [](firebase::App *app, void* userData) {
				auto self = static_cast<FlFirebaseAuthPlugin*>(userData);
				firebase::InitResult result;
				firebase::auth::Auth::GetAuth(app, &result);
				self->authInitializedApps.insert(app->name());
				return result;
			});

	while (initializer.InitializeLastResult().status() != firebase::kFutureStatusComplete) {
		std::this_thread::yield();
	}

	if (initializer.InitializeLastResult().error() != 0) {
		exit(1);
	}

}

static FlValue* firebase_variant_to_fl_value(firebase::Variant const& variant) {
	FlValue *value = nullptr;
	switch (variant.type()) {
		case firebase::Variant::kTypeNull:
			value = fl_value_new_null();
			break;
		case firebase::Variant::kTypeInt64:
			value = fl_value_new_int(variant.int64_value());
			break;
		case firebase::Variant::kTypeDouble:
			value = fl_value_new_float(variant.double_value());
			break;
		case firebase::Variant::kTypeBool:
			value = fl_value_new_bool(variant.bool_value());
			break;
		case firebase::Variant::kTypeStaticString:
		case firebase::Variant::kTypeMutableString:
			value = fl_value_new_string(variant.string_value());
			break;
		case firebase::Variant::kTypeVector:
			value = fl_value_new_list();
			for (auto elem : variant.vector()) {
				fl_value_append_take(value, firebase_variant_to_fl_value(elem));
			}
			break;
		case firebase::Variant::kTypeMap:
			value = fl_value_new_map();
			for (auto pair : variant.map()) {
				fl_value_set_take(value, firebase_variant_to_fl_value(pair.first), firebase_variant_to_fl_value(pair.second));
			}
			break;
		case firebase::Variant::kTypeStaticBlob:
		case firebase::Variant::kTypeMutableBlob:
			value = fl_value_new_uint8_list(variant.blob_data(), variant.blob_size());
			break;
	}

	return value;
}

static FlValue* firebase_profile_to_map(std::map<firebase::Variant, firebase::Variant> const& profile) {
	FlValue *profileMap = fl_value_new_map();

	for (auto pair : profile) {
		fl_value_set_take(profileMap, firebase_variant_to_fl_value(pair.first), firebase_variant_to_fl_value(pair.second));
	}

	return profileMap;
}

static FlValue* firebase_additional_user_info_to_map(firebase::auth::AdditionalUserInfo const *additionalUserInfo) {
	FlValue *additionalUserInfoMap = fl_value_new_map();

	// TODO: This information isn't available in the AdditionalUserInfo struct, maybe we have to pass this value in
	fl_value_set_take(additionalUserInfoMap,
			fl_value_new_string("isNewUser"),
			fl_value_new_bool(false));

	fl_value_set_take(additionalUserInfoMap,
			fl_value_new_string("profile"),
			firebase_profile_to_map(additionalUserInfo->profile));

	fl_value_set_take(additionalUserInfoMap,
			fl_value_new_string(kArgumentProviderId),
			fl_value_new_string(additionalUserInfo->provider_id.c_str()));

	fl_value_set_take(additionalUserInfoMap,
			fl_value_new_string("username"),
			fl_value_new_string(additionalUserInfo->user_name.c_str()));

	return additionalUserInfoMap;
}

static FlValue* firebase_user_info_to_map(firebase::auth::UserInfoInterface const *userInfo) {
	FlValue *userMap = fl_value_new_map();

	// TODO: Check for empty strings?
	fl_value_set_take(userMap,
			fl_value_new_string(kArgumentProviderId),
			fl_value_new_string(userInfo->provider_id().c_str()));

	fl_value_set_take(userMap,
			fl_value_new_string("displayName"),
			fl_value_new_string(userInfo->display_name().c_str()));

	fl_value_set_take(userMap,
			fl_value_new_string("uid"),
			fl_value_new_string(userInfo->uid().c_str()));

	fl_value_set_take(userMap,
			fl_value_new_string("photoUrl"),
			fl_value_new_string(userInfo->photo_url().c_str()));

	fl_value_set_take(userMap,
			fl_value_new_string(kArgumentEmail),
			fl_value_new_string(userInfo->email().c_str()));

	fl_value_set_take(userMap,
			fl_value_new_string("phoneNumber"),
			fl_value_new_string(userInfo->phone_number().c_str()));

	return userMap;

}

static FlValue* firebase_user_to_map(firebase::auth::User const *user) {
	if (!user) {
		return fl_value_new_null();
	}

	FlValue *userMap = firebase_user_info_to_map(user);
	FlValue *metadataMap = fl_value_new_map();

	// TODO: Check for empty strings?
	fl_value_set_take(userMap,
			fl_value_new_string("creationTime"),
			fl_value_new_int(static_cast<int64_t>(user->metadata().creation_timestamp)));

	fl_value_set_take(metadataMap,
			fl_value_new_string("lastSignInTime"),
			fl_value_new_int(static_cast<int64_t>(user->metadata().last_sign_in_timestamp)));

	fl_value_set_take(userMap,
			fl_value_new_string("metadata"),
			metadataMap);

	FlValue *providerArray = fl_value_new_list();

	for (firebase::auth::UserInfoInterface *providerData : user->provider_data()) {
		fl_value_append_take(providerArray, firebase_user_info_to_map(providerData));
	}

	fl_value_set_take(userMap,
			fl_value_new_string("providerData"),
			providerArray);

	fl_value_set_take(userMap,
			fl_value_new_string("isAnonymous"),
			fl_value_new_bool(user->is_anonymous()));

	fl_value_set_take(userMap,
			fl_value_new_string("emailVerified"),
			fl_value_new_bool(user->is_email_verified()));

	// Native does not provide refresh tokens
	fl_value_set_take(userMap,
			fl_value_new_string("refreshToken"),
			fl_value_new_string(""));

	return userMap;
}

static FlValue* firebase_credential_to_map(FlFirebaseAuthPlugin *self, firebase::auth::Credential const *credential) {
	FlValue *credentialMap = fl_value_new_map();

	fl_value_set_take(credentialMap,
			fl_value_new_string(kArgumentProviderId),
			fl_value_new_string(credential->provider().c_str()));

	fl_value_set_take(credentialMap,
			fl_value_new_string(kArgumentSignInMethod),
			fl_value_new_string(credential->provider().c_str()));

	// Generator a new random token to assign to this set of credentials and store the values in the userCredentials map
	uint64_t token = std::random_device{}();
	self->userCredentials[token] = *credential;

	fl_value_set_take(credentialMap,
			fl_value_new_string(kArgumentToken),
			fl_value_new_int(token));

	return credentialMap;
}

static FlValue* firebase_sign_in_result_to_map(FlFirebaseAuthPlugin *self, firebase::auth::SignInResult const *signInResult) {
	FlValue *authResultMap = fl_value_new_map();

	fl_value_set_take(authResultMap,
			fl_value_new_string("additionalUserInfo"),
			firebase_additional_user_info_to_map(&signInResult->info));

	fl_value_set_take(authResultMap,
			fl_value_new_string("authCredential"),
			signInResult->info.updated_credential.is_valid() ?
				firebase_credential_to_map(self, &signInResult->info.updated_credential) :
				fl_value_new_null());

	fl_value_set_take(authResultMap,
			fl_value_new_string("user"),
			firebase_user_to_map(signInResult->user));

	return authResultMap;
}

MyAuthStateListener::MyAuthStateListener(FlFirebaseAuthPlugin *plugin) : _pluginState{ plugin } {}

void MyAuthStateListener::OnAuthStateChanged(firebase::auth::Auth *auth) {
	// Call Auth#authStateChanges and pass appName and user as a dictionary

	FlValue *args = fl_value_new_map();
	fl_value_set_take(args, fl_value_new_string("appName"), fl_value_new_string(get_firebase_app_name(auth->app().name())));
	fl_value_set_take(args, fl_value_new_string("user"), firebase_user_to_map(auth->current_user()));
	fl_method_channel_invoke_method(
			_pluginState->channel,
			"Auth#authStateChanges",
			args, nullptr, nullptr, nullptr);
}

MyIdTokenListener::MyIdTokenListener(FlFirebaseAuthPlugin *plugin) : _pluginState{ plugin } {}

void MyIdTokenListener::OnIdTokenChanged(firebase::auth::Auth *auth) {
	// Call Auth#idTokenChanges and pass appName and user as a dictionary

	FlValue *args = fl_value_new_map();
	fl_value_set_take(args, fl_value_new_string("appName"), fl_value_new_string(get_firebase_app_name(auth->app().name())));
	fl_value_set_take(args, fl_value_new_string("user"), firebase_user_to_map(auth->current_user()));
	fl_method_channel_invoke_method(
			_pluginState->channel,
			"Auth#idTokenChanges",
			args, nullptr, nullptr, nullptr);
}

static firebase::auth::Auth* get_auth_for_app_with_name(char const *appName) {
	firebase::App *app = firebase::App::GetInstance(get_internal_app_name(appName));

	if (!app) {
		g_warning("Failed to retrive app with name: %s", appName);
		return nullptr;
	}

	firebase::InitResult initResult;
	firebase::auth::Auth *auth = firebase::auth::Auth::GetAuth(app, &initResult);
	if (initResult != firebase::kInitResultSuccess) {
		g_warning("Failed to init auth");
	}

	return auth;
}

static FlMethodResponse* auth_register_change_listeners(FlFirebaseAuthPlugin *self, FlValue *args) {

	FlValue *appNameValue = fl_value_lookup_string(args, "appName");
	g_warning("Registering change listeners for app: %s", fl_value_get_string(appNameValue));

	ensure_auth_initialized(self, fl_value_get_string(appNameValue));
	firebase::auth::Auth *auth = get_auth_for_app_with_name(fl_value_get_string(appNameValue));

	auth->AddAuthStateListener(self->authListener.get());
	auth->AddIdTokenListener(self->idListener.get());

	return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

static FlMethodResponse* auth_apply_action_code(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_confirm_password_reset(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_create_user_with_email_and_password(FlFirebaseAuthPlugin *self, FlValue *args) {
	FlValue *appNameValue = fl_value_lookup_string(args, "appName");
	FlValue *emailValue = fl_value_lookup_string(args, kArgumentEmail);
	FlValue *passwordValue = fl_value_lookup_string(args, "password");

	firebase::auth::Auth *auth = get_auth_for_app_with_name(fl_value_get_string(appNameValue));

	firebase::Future<firebase::auth::User*> userFuture = auth->CreateUserWithEmailAndPassword(
			fl_value_get_string(emailValue),
			fl_value_get_string(passwordValue));


	while (userFuture.status() == firebase::kFutureStatusPending) {
		std::this_thread::yield();
	}

	firebase::auth::AuthError error = static_cast<firebase::auth::AuthError>(userFuture.error());
	if (error != firebase::auth::AuthError::kAuthErrorNone) {
		g_error("Firebase auth error: %s", userFuture.error_message());
		return FL_METHOD_RESPONSE(fl_method_error_response_new(kAuthError, userFuture.error_message(), nullptr));
	}

	firebase::auth::User *user = *userFuture.result();

	firebase::Future<firebase::auth::SignInResult> signInResultFuture =
		user->ReauthenticateAndRetrieveData(
				firebase::auth::EmailAuthProvider::GetCredential(
					fl_value_get_string(emailValue),
					fl_value_get_string(passwordValue)));

	while (signInResultFuture.status() == firebase::kFutureStatusPending) {
		std::this_thread::yield();
	}

	error = static_cast<firebase::auth::AuthError>(signInResultFuture.error());
	if (error != firebase::auth::AuthError::kAuthErrorNone) {
		g_error("Firebase auth error: %s", signInResultFuture.error_message());
		return FL_METHOD_RESPONSE(fl_method_error_response_new(kAuthError, signInResultFuture.error_message(), nullptr));
	}

	firebase::auth::SignInResult signInResult = *signInResultFuture.result();

	return FL_METHOD_RESPONSE(fl_method_success_response_new(firebase_sign_in_result_to_map(self, &signInResult)));
}

static FlMethodResponse* auth_fetch_sign_in_methods_for_email(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_send_password_reset_email(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_send_sign_in_link_to_email(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_sign_in_with_credential(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_set_language_code(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_set_settings(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_sign_in_anonymously(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_sign_in_with_custom_token(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_sign_in_with_email_and_password(FlFirebaseAuthPlugin *self, FlValue *args) {
	if (fl_value_get_type(args) != FL_VALUE_TYPE_MAP
			|| fl_value_get_length(args) != 3) {
		return FL_METHOD_RESPONSE(fl_method_error_response_new(kBadArgumentsError, "Expected 3 string arguments", nullptr));
	}

	FlValue *appNameValue = fl_value_lookup_string(args, "appName");
	FlValue *emailValue = fl_value_lookup_string(args, "email");
	FlValue *passwordValue = fl_value_lookup_string(args, "password");

	if (fl_value_get_type(appNameValue) != FL_VALUE_TYPE_STRING
			|| fl_value_get_type(emailValue) != FL_VALUE_TYPE_STRING
			|| fl_value_get_type(passwordValue) != FL_VALUE_TYPE_STRING) {
		return FL_METHOD_RESPONSE(fl_method_error_response_new(kBadArgumentsError, "Expected 3 string arguments", nullptr));
	}

	firebase::auth::Auth *auth = get_auth_for_app_with_name(fl_value_get_string(appNameValue));

	firebase::Future<firebase::auth::SignInResult> signInResultFuture =
		auth->SignInAndRetrieveDataWithCredential(
				firebase::auth::EmailAuthProvider::GetCredential(
					fl_value_get_string(emailValue),
					fl_value_get_string(passwordValue)));

	while (signInResultFuture.status() == firebase::kFutureStatusPending) {
		// Poll events
		// TODO: Is there a way to push this to the glib main loop so that our thread isn't blocked?
		// Is that even necessary, does the flutter engine run this plugin in it's own thread?
		std::this_thread::yield();
	}

	firebase::auth::AuthError error = static_cast<firebase::auth::AuthError>(signInResultFuture.error());
	if (error != firebase::auth::AuthError::kAuthErrorNone) {
		g_error("Firebase auth error: %s", signInResultFuture.error_message());
		return FL_METHOD_RESPONSE(fl_method_error_response_new(kAuthError, signInResultFuture.error_message(), nullptr));
	}

	firebase::auth::SignInResult signInResult = *signInResultFuture.result();

	return FL_METHOD_RESPONSE(fl_method_success_response_new(firebase_sign_in_result_to_map(self, &signInResult)));
}

static FlMethodResponse* auth_sign_in_with_email_link(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_sign_out(FlFirebaseAuthPlugin *self, FlValue *args) {
	if (fl_value_get_type(args) != FL_VALUE_TYPE_MAP
			|| fl_value_get_length(args) != 1) {
		return FL_METHOD_RESPONSE(fl_method_error_response_new(kBadArgumentsError, "Expected 1 string arguments", nullptr));
	}

	FlValue *appNameValue = fl_value_lookup_string(args, "appName");

	if (fl_value_get_type(appNameValue) != FL_VALUE_TYPE_STRING) {
		return FL_METHOD_RESPONSE(fl_method_error_response_new(kBadArgumentsError, "Expected 1 string arguments", nullptr));
	}

	firebase::auth::Auth *auth = get_auth_for_app_with_name(fl_value_get_string(appNameValue));

	auth->SignOut();

	return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

static FlMethodResponse* auth_verify_password_reset_code(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* auth_verify_phone_number(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_delete(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_get_id_token(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_link_with_credential(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_reauthenticateUserWithCredential(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_reload(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_send_email_verification(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_unlink(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_update_email(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_update_password(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_update_phone_number(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_update_profile(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

static FlMethodResponse* user_verify_before_update_email(FlFirebaseAuthPlugin *self, FlValue *args) {
	return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

// Called when a method call is received from Flutter.
static void method_call_cb(FlMethodChannel* channel, FlMethodCall* method_call,
                           gpointer user_data) {
  FlFirebaseAuthPlugin* self = FL_FIREBASE_AUTH_PLUGIN(user_data);

  const gchar* method = fl_method_call_get_name(method_call);
  FlValue* args = fl_method_call_get_args(method_call);

  g_autoptr(FlMethodResponse) response = nullptr;
  if (strcmp(method, "Auth#registerChangeListeners") == 0) {
	  response = auth_register_change_listeners(self, args);
  } else if (strcmp(method, "Auth#applyActionCode") == 0) {
	  response = auth_apply_action_code(self, args);
  } else if (strcmp(method, "Auth#confirmPasswordReset") == 0) {
	  response = auth_confirm_password_reset(self, args);
  } else if (strcmp(method, "Auth#createUserWithEmailAndPassword") == 0) {
	  response = auth_create_user_with_email_and_password(self, args);
  } else if (strcmp(method, "Auth#fetchSignInMethodsForEmail") == 0) {
	  response = auth_fetch_sign_in_methods_for_email(self, args);
  } else if (strcmp(method, "Auth#sendPasswordResetEmail") == 0) {
	  response = auth_send_password_reset_email(self, args);
  } else if (strcmp(method, "Auth#sendSignInLinkToEmail") == 0) {
	  response = auth_send_sign_in_link_to_email(self, args);
  } else if (strcmp(method, "Auth#signInWithCredential") == 0) {
	  response = auth_sign_in_with_credential(self, args);
  } else if (strcmp(method, "Auth#setLanguageCode") == 0) {
	  response = auth_set_language_code(self, args);
  } else if (strcmp(method, "Auth#setSettings") == 0) {
	  response = auth_set_settings(self, args);
  } else if (strcmp(method, "Auth#signInAnonymously") == 0) {
	  response = auth_sign_in_anonymously(self, args);
  } else if (strcmp(method, "Auth#signInWithCustomToken") == 0) {
	  response = auth_sign_in_with_custom_token(self, args);
  } else if (strcmp(method, "Auth#signInWithEmailAndPassword") == 0) {
	  response = auth_sign_in_with_email_and_password(self, args);
  } else if (strcmp(method, "Auth#signInWithEmailLink") == 0) {
	  response = auth_sign_in_with_email_link(self, args);
  } else if (strcmp(method, "Auth#signOut") == 0) {
	  response = auth_sign_out(self, args);
  } else if (strcmp(method, "Auth#verifyPasswordResetCode") == 0) {
	  response = auth_verify_password_reset_code(self, args);
  } else if (strcmp(method, "Auth#verifyPhoneNumber") == 0) {
	  response = auth_verify_phone_number(self, args);
  } else if (strcmp(method, "User#delete") == 0) {
	  response = user_delete(self, args);
  } else if (strcmp(method, "User#getIdToken") == 0) {
	  response = user_get_id_token(self, args);
  } else if (strcmp(method, "User#linkWithCredential") == 0) {
	  response = user_link_with_credential(self, args);
  } else if (strcmp(method, "User#reauthenticateUserWithCredential") == 0) {
	  response = user_reauthenticateUserWithCredential(self, args);
  } else if (strcmp(method, "User#reload") == 0) {
	  response = user_reload(self, args);
  } else if (strcmp(method, "User#sendEmailVerification") == 0) {
	  response = user_send_email_verification(self, args);
  } else if (strcmp(method, "User#unlink") == 0) {
	  response = user_unlink(self, args);
  } else if (strcmp(method, "User#updateEmail") == 0) {
	  response = user_update_email(self, args);
  } else if (strcmp(method, "User#updatePassword") == 0) {
	  response = user_update_password(self, args);
  } else if (strcmp(method, "User#updatePhoneNumber") == 0) {
	  response = user_update_phone_number(self, args);
  } else if (strcmp(method, "User#updateProfile") == 0) {
	  response = user_update_profile(self, args);
  } else if (strcmp(method, "User#verifyBeforeUpdateEmail") == 0) {
	  response = user_verify_before_update_email(self, args);
  } else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  g_autoptr(GError) error = nullptr;
  if (!fl_method_call_respond(method_call, response, &error))
    g_warning("Failed to send method call response: %s", error->message);
}

static void fl_firebase_auth_plugin_dispose(GObject* object) {
  FlFirebaseAuthPlugin* self = FL_FIREBASE_AUTH_PLUGIN(object);

  g_clear_object(&self->registrar);
  g_clear_object(&self->channel);

  G_OBJECT_CLASS(fl_firebase_auth_plugin_parent_class)->dispose(object);
}

static void fl_firebase_auth_plugin_class_init(FlFirebaseAuthPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = fl_firebase_auth_plugin_dispose;
}

static void fl_firebase_auth_plugin_init(FlFirebaseAuthPlugin* self) {
	// This function gets called as apart of the gobject initialization, do plugin init here

	self->authListener = std::make_unique<MyAuthStateListener>(self);
	self->idListener = std::make_unique<MyIdTokenListener>(self);
	self->userCredentials = std::unordered_map<uint64_t, firebase::auth::Credential>{};
	self->authInitializedApps = std::unordered_set<std::string>{};
}

FlFirebaseAuthPlugin* fl_firebase_auth_plugin_new(FlPluginRegistrar* registrar) {
  FlFirebaseAuthPlugin* self = FL_FIREBASE_AUTH_PLUGIN(
      g_object_new(fl_firebase_auth_plugin_get_type(), nullptr));

  self->registrar = FL_PLUGIN_REGISTRAR(g_object_ref(registrar));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  self->channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            kChannelName, FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(self->channel, method_call_cb,
                                            g_object_ref(self), g_object_unref);

  return self;
}

void fl_firebase_auth_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  FlFirebaseAuthPlugin* plugin = fl_firebase_auth_plugin_new(registrar);
  g_object_unref(plugin);
}
