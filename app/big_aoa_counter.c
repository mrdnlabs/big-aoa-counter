#include "civetweb.h"
#include <axoverlay.h>
#include <axsdk/axparameter.h>
#include <cairo/cairo.h>
#include <curl/curl.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <glib.h>
#include <jansson.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define APP_NAME "big_aoa_counter"
#define HTTP_PORT "2001"
#define MAX_BODY_SIZE 16384

typedef struct {
    gchar* label;
    gchar* mode;
    gchar* dynamic_text_slot;
    gchar* scenario_uid;
    gchar* category;
    gchar* poll_interval_ms;
} AppConfig;

typedef struct {
    pthread_mutex_t mutex;
    gint count;
    gchar* label;
    gchar* scenario_uid;
    gchar* scenario_name;
    gchar* scenario_type;
    gchar* category;
    gchar* timestamp;
    gchar* reset_time;
    gchar* api_version;
    gchar* last_error;
} AppState;

static volatile sig_atomic_t application_running = 1;
static gint overlay_id = -1;
static AXParameter* parameter_handle = NULL;
static struct mg_context* web_context = NULL;
static GMainLoop* main_loop = NULL;
static AppState app_state;

__attribute__((noreturn)) __attribute__((format(printf, 1, 2))) static void
panic(const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    vsyslog(LOG_ERR, format, arg);
    va_end(arg);
    exit(1);
}

static void state_replace(gchar** field, const char* value) {
    g_free(*field);
    *field = g_strdup(value ? value : "");
}

static void app_config_init(AppConfig* config) {
    memset(config, 0, sizeof(*config));
}

static void app_config_clear(AppConfig* config) {
    g_free(config->label);
    g_free(config->mode);
    g_free(config->dynamic_text_slot);
    g_free(config->scenario_uid);
    g_free(config->category);
    g_free(config->poll_interval_ms);
}

static gboolean read_parameter(const char* name, gchar** value) {
    GError* error = NULL;
    if (!ax_parameter_get(parameter_handle, name, value, &error)) {
        syslog(LOG_ERR, "Failed reading parameter %s: %s", name, error->message);
        g_error_free(error);
        return FALSE;
    }
    return TRUE;
}

static AppConfig load_config(void) {
    AppConfig config;
    app_config_init(&config);
    if (!read_parameter("Label", &config.label) || !read_parameter("Mode", &config.mode) ||
        !read_parameter("DynamicTextSlot", &config.dynamic_text_slot) ||
        !read_parameter("ScenarioUid", &config.scenario_uid) ||
        !read_parameter("Category", &config.category) ||
        !read_parameter("PollIntervalMs", &config.poll_interval_ms)) {
        panic("Failed to load application parameters");
    }
    return config;
}

static gboolean persist_config_value(const char* key, const char* value) {
    GError* error = NULL;
    if (!ax_parameter_set(parameter_handle, key, value, TRUE, &error)) {
        syslog(LOG_ERR, "Failed writing parameter %s: %s", key, error->message);
        g_error_free(error);
        return FALSE;
    }
    return TRUE;
}

static void stop_application(int status) {
    (void)status;
    application_running = 0;
    if (main_loop != NULL) {
        g_main_loop_quit(main_loop);
    }
}

static gboolean signal_handler(gpointer loop) {
    application_running = 0;
    g_main_loop_quit((GMainLoop*)loop);
    return G_SOURCE_REMOVE;
}

static void setup_overlay_data(struct axoverlay_overlay_data* data) {
    axoverlay_init_overlay_data(data);
    data->postype = AXOVERLAY_CUSTOM_NORMALIZED;
    data->anchor_point = AXOVERLAY_ANCHOR_CENTER;
    data->x = 0.0;
    data->y = -0.72;
    data->scale_to_stream = FALSE;
}

static void adjustment_cb(gint id,
                          struct axoverlay_stream_data* stream,
                          enum axoverlay_position_type* postype,
                          gfloat* overlay_x,
                          gfloat* overlay_y,
                          gint* overlay_width,
                          gint* overlay_height,
                          gpointer user_data) {
    (void)id;
    (void)postype;
    (void)overlay_x;
    (void)overlay_y;
    (void)user_data;
    *overlay_width = stream->rotation == 90 || stream->rotation == 270 ? stream->height : stream->width;
    *overlay_height = MAX(140, *overlay_width / 7);
}

static void draw_overlay(gpointer rendering_context,
                         gint id,
                         struct axoverlay_stream_data* stream,
                         enum axoverlay_position_type postype,
                         gfloat overlay_x,
                         gfloat overlay_y,
                         gint overlay_width,
                         gint overlay_height,
                         gpointer user_data) {
    (void)id;
    (void)stream;
    (void)postype;
    (void)overlay_x;
    (void)overlay_y;
    (void)user_data;

    gchar* label = NULL;
    gchar* scenario = NULL;
    gint count = 0;

    pthread_mutex_lock(&app_state.mutex);
    label = g_strdup(app_state.label && *app_state.label ? app_state.label : "Object count");
    scenario = g_strdup(app_state.scenario_name && *app_state.scenario_name ? app_state.scenario_name
                                                                            : "Awaiting AOA scenario");
    count = app_state.count;
    pthread_mutex_unlock(&app_state.mutex);

    cairo_t* cr = rendering_context;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    cairo_set_source_rgba(cr, 0.06, 0.06, 0.06, 0.72);
    cairo_new_path(cr);
    cairo_arc(cr, 28, 28, 28, G_PI, 3 * G_PI / 2);
    cairo_arc(cr, overlay_width - 28, 28, 28, 3 * G_PI / 2, 0);
    cairo_arc(cr, overlay_width - 28, overlay_height - 28, 28, 0, G_PI / 2);
    cairo_arc(cr, 28, overlay_height - 28, 28, G_PI / 2, G_PI);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.94, 0.50, 0.18, 1.0);
    cairo_rectangle(cr, 16, 16, 14, overlay_height - 32);
    cairo_fill(cr);

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_source_rgb(cr, 0.97, 0.96, 0.93);
    cairo_set_font_size(cr, overlay_height * 0.48);
    gchar* count_text = g_strdup_printf("%d", count);
    cairo_move_to(cr, 52, overlay_height * 0.66);
    cairo_show_text(cr, count_text);

    cairo_set_font_size(cr, overlay_height * 0.18);
    cairo_move_to(cr, overlay_width * 0.36, overlay_height * 0.42);
    cairo_show_text(cr, label);

    cairo_set_font_size(cr, overlay_height * 0.12);
    cairo_set_source_rgba(cr, 0.85, 0.85, 0.82, 0.95);
    cairo_move_to(cr, overlay_width * 0.36, overlay_height * 0.68);
    cairo_show_text(cr, scenario);

    g_free(count_text);
    g_free(label);
    g_free(scenario);
}

static gboolean redraw_overlay(void) {
    GError* error = NULL;
    axoverlay_redraw(&error);
    if (error != NULL) {
        syslog(LOG_ERR, "axoverlay redraw failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    return TRUE;
}

static void set_state_error(const char* error_message) {
    pthread_mutex_lock(&app_state.mutex);
    state_replace(&app_state.last_error, error_message);
    pthread_mutex_unlock(&app_state.mutex);
}

static size_t append_to_gstring_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t processed_bytes = size * nmemb;
    g_string_append_len((GString*)userdata, ptr, processed_bytes);
    return processed_bytes;
}

static char* parse_credentials(GVariant* result) {
    char* credentials_string = NULL;
    char* user = NULL;
    char* password = NULL;
    g_variant_get(result, "(&s)", &credentials_string);
    if (sscanf(credentials_string, "%m[^:]:%ms", &user, &password) != 2) {
        return NULL;
    }
    char* credentials = g_strdup_printf("%s:%s", user, password);
    free(user);
    free(password);
    return credentials;
}

static char* retrieve_vapix_credentials(const char* username) {
    GError* error = NULL;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!connection) {
        syslog(LOG_WARNING, "DBus unavailable for VAPIX credentials: %s", error->message);
        g_error_free(error);
        return NULL;
    }

    GVariant* result = g_dbus_connection_call_sync(connection,
                                                   "com.axis.HTTPConf1",
                                                   "/com/axis/HTTPConf1/VAPIXServiceAccounts1",
                                                   "com.axis.HTTPConf1.VAPIXServiceAccounts1",
                                                   "GetCredentials",
                                                   g_variant_new("(s)", username),
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);
    g_object_unref(connection);
    if (!result) {
        syslog(LOG_WARNING, "Failed to retrieve VAPIX service credentials: %s", error->message);
        g_error_free(error);
        return NULL;
    }

    char* credentials = parse_credentials(result);
    g_variant_unref(result);
    return credentials;
}

static char* build_runtime_credentials(const AppConfig* config) {
    (void)config;
    return retrieve_vapix_credentials("bacounter");
}

static char* vapix_post(CURL* handle,
                        const AppConfig* config,
                        const char* endpoint,
                        const char* request,
                        const char* credentials) {
    const char* default_hosts[] = {"http://127.0.0.12", "http://127.0.1.1", "http://127.0.0.1"};
    const char** hosts = default_hosts;
    guint host_count = G_N_ELEMENTS(default_hosts);
    (void)config;
    for (guint i = 0; i < host_count; i++) {
        GString* response = g_string_new(NULL);
        gchar* url = g_strdup_printf("%s%s", hosts[i], endpoint);
        curl_easy_reset(handle);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, append_to_gstring_callback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, response);
        if (credentials && *credentials) {
            curl_easy_setopt(handle, CURLOPT_USERPWD, credentials);
            curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        }
        CURLcode res = curl_easy_perform(handle);
        long response_code = 0;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
        if (res == CURLE_OK && response_code == 200) {
            g_free(url);
            return g_string_free(response, FALSE);
        }
        syslog(LOG_WARNING, "Request to %s failed: curl=%d http=%ld", url, res, response_code);
        g_free(url);
        g_string_free(response, TRUE);
    }
    return NULL;
}

static gboolean vapix_get(CURL* handle,
                          const AppConfig* config,
                          const char* endpoint_with_query,
                          const char* credentials) {
    const char* default_hosts[] = {"http://127.0.0.12", "http://127.0.1.1", "http://127.0.0.1"};
    const char** hosts = default_hosts;
    guint host_count = G_N_ELEMENTS(default_hosts);
    (void)config;

    for (guint i = 0; i < host_count; i++) {
        gchar* url = g_strdup_printf("%s%s", hosts[i], endpoint_with_query);
        curl_easy_reset(handle);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 10L);
        if (credentials && *credentials) {
            curl_easy_setopt(handle, CURLOPT_USERPWD, credentials);
            curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        }
        CURLcode res = curl_easy_perform(handle);
        long response_code = 0;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
        g_free(url);
        if (res == CURLE_OK && response_code == 200) {
            return TRUE;
        }
    }
    return FALSE;
}

static json_t* vapix_post_json(CURL* handle,
                               const AppConfig* config,
                               const char* endpoint,
                               const char* request,
                               const char* credentials) {
    char* text_response = vapix_post(handle, config, endpoint, request, credentials);
    if (text_response == NULL) {
        return NULL;
    }
    json_error_t error;
    json_t* response = json_loads(text_response, 0, &error);
    g_free(text_response);
    if (!response) {
        syslog(LOG_ERR, "Invalid JSON from %s: %s", endpoint, error.text);
        return NULL;
    }
    return response;
}

static gchar* determine_api_version(CURL* handle, const AppConfig* config, const char* credentials) {
    const char* request = "{\"context\":\"big_aoa_counter\",\"method\":\"getSupportedVersions\"}";
    json_t* response =
        vapix_post_json(handle, config, "/local/objectanalytics/control.cgi", request, credentials);
    if (!response) {
        return g_strdup("1.2");
    }
    json_t* data = json_object_get(response, "data");
    json_t* versions = data ? json_object_get(data, "apiVersions") : NULL;
    gchar* version = g_strdup("1.2");
    if (json_is_array(versions) && json_array_size(versions) > 0) {
        const json_t* last = json_array_get(versions, json_array_size(versions) - 1);
        if (json_is_string(last)) {
            g_free(version);
            version = g_strdup(json_string_value(last));
        }
    }
    json_decref(response);
    return version;
}

static gboolean parse_scenario_from_config(json_t* response,
                                           const AppConfig* config,
                                           gchar** scenario_uid,
                                           gchar** scenario_name,
                                           gchar** scenario_type) {
    json_t* data = json_object_get(response, "data");
    json_t* scenarios = data ? json_object_get(data, "scenarios") : NULL;
    if (!json_is_array(scenarios)) {
        return FALSE;
    }

    const char* requested_uid = config->scenario_uid;
    for (size_t i = 0; i < json_array_size(scenarios); i++) {
        json_t* scenario = json_array_get(scenarios, i);
        json_t* id = json_object_get(scenario, "id");
        json_t* name = json_object_get(scenario, "name");
        json_t* type = json_object_get(scenario, "type");
        gchar id_buffer[64];
        if (!json_is_integer(id) || !json_is_string(name) || !json_is_string(type)) {
            continue;
        }
        const char* type_value = json_string_value(type);
        gboolean is_countable = g_strcmp0(type_value, "crosslinecounting") == 0 ||
                                g_strcmp0(type_value, "occupancyInArea") == 0;
        if (!is_countable) {
            continue;
        }
        g_snprintf(id_buffer, sizeof(id_buffer), "%" JSON_INTEGER_FORMAT, json_integer_value(id));
        gboolean uid_matches = requested_uid == NULL || *requested_uid == '\0' ||
                               g_strcmp0(requested_uid, id_buffer) == 0;
        if (uid_matches) {
            *scenario_uid = g_strdup(id_buffer);
            *scenario_name = g_strdup(json_string_value(name));
            *scenario_type = g_strdup(type_value);
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean discover_scenario(CURL* handle,
                                  const AppConfig* config,
                                  const char* api_version,
                                  const char* credentials,
                                  gchar** scenario_uid,
                                  gchar** scenario_name,
                                  gchar** scenario_type) {
    gchar* request = g_strdup_printf(
        "{\"apiVersion\":\"%s\",\"context\":\"big_aoa_counter\",\"method\":\"getConfiguration\"}",
        api_version);
    json_t* response =
        vapix_post_json(handle, config, "/local/objectanalytics/control.cgi", request, credentials);
    g_free(request);
    if (!response) {
        return FALSE;
    }
    gboolean result =
        parse_scenario_from_config(response, config, scenario_uid, scenario_name, scenario_type);
    json_decref(response);
    return result;
}

static json_t* build_available_scenarios_json(CURL* handle,
                                              const AppConfig* config,
                                              const char* api_version,
                                              const char* credentials) {
    gchar* request = g_strdup_printf(
        "{\"apiVersion\":\"%s\",\"context\":\"big_aoa_counter\",\"method\":\"getConfiguration\"}",
        api_version);
    json_t* response =
        vapix_post_json(handle, config, "/local/objectanalytics/control.cgi", request, credentials);
    g_free(request);
    json_t* result = json_array();
    if (!response) {
        return result;
    }

    json_t* data = json_object_get(response, "data");
    json_t* scenarios = data ? json_object_get(data, "scenarios") : NULL;
    if (json_is_array(scenarios)) {
        for (size_t i = 0; i < json_array_size(scenarios); i++) {
            json_t* scenario = json_array_get(scenarios, i);
            json_t* id = json_object_get(scenario, "id");
            json_t* name = json_object_get(scenario, "name");
            json_t* type = json_object_get(scenario, "type");
            gchar id_buffer[64];
            if (!json_is_integer(id) || !json_is_string(name) || !json_is_string(type)) {
                continue;
            }

            const char* type_value = json_string_value(type);
            gboolean is_countable = g_strcmp0(type_value, "crosslinecounting") == 0 ||
                                    g_strcmp0(type_value, "occupancyInArea") == 0;
            if (!is_countable) {
                continue;
            }

            g_snprintf(id_buffer, sizeof(id_buffer), "%" JSON_INTEGER_FORMAT, json_integer_value(id));

            json_array_append_new(
                result,
                json_pack("{s:s,s:s,s:s}",
                          "id",
                          id_buffer,
                          "name",
                          json_string_value(name),
                          "type",
                          type_value));
        }
    }

    json_decref(response);
    return result;
}

static gboolean update_dynamic_text(CURL* handle,
                                    const char* credentials,
                                    const AppConfig* config,
                                    gint count) {
    gint slot = CLAMP((gint)g_ascii_strtoll(config->dynamic_text_slot, NULL, 10), 1, 16);
    gchar* raw_text = g_strdup_printf("%s %d", config->label, count);
    gchar* text = g_uri_escape_string(raw_text, NULL, TRUE);
    gchar* query = g_strdup_printf("/axis-cgi/dynamicoverlay.cgi?action=settext&text=%s&text_index=%d",
                                   text,
                                   slot);
    gboolean ok = vapix_get(handle, config, query, credentials);
    g_free(query);
    g_free(raw_text);
    g_free(text);
    return ok;
}

static gboolean update_count_from_aoa(CURL* handle,
                                      const AppConfig* config,
                                      const gchar* api_version,
                                      const char* credentials) {
    gchar* scenario_uid = NULL;
    gchar* scenario_name = NULL;
    gchar* scenario_type = NULL;
    if (!discover_scenario(handle, config, api_version, credentials, &scenario_uid, &scenario_name,
                           &scenario_type)) {
        set_state_error("No AOA counting scenario found");
        return FALSE;
    }

    const char* method = g_strcmp0(scenario_type, "occupancyInArea") == 0 ? "getOccupancy"
                                                                           : "getAccumulatedCounts";
    gchar* request = g_strdup_printf(
        "{\"apiVersion\":\"%s\",\"context\":\"big_aoa_counter\",\"method\":\"%s\",\"params\":{\"scenario\":%s}}",
        api_version,
        method,
        scenario_uid);
    json_t* response =
        vapix_post_json(handle, config, "/local/objectanalytics/control.cgi", request, credentials);
    g_free(request);
    if (!response) {
        g_free(scenario_uid);
        g_free(scenario_name);
        g_free(scenario_type);
        set_state_error("AOA count request failed");
        return FALSE;
    }

    json_t* error = json_object_get(response, "error");
    if (error) {
        json_t* message = json_object_get(error, "message");
        set_state_error(json_string_value(message));
        json_decref(response);
        g_free(scenario_uid);
        g_free(scenario_name);
        g_free(scenario_type);
        return FALSE;
    }

    json_t* data = json_object_get(response, "data");
    gint count = 0;
    if (data && g_strcmp0(config->category, "totalVehicle") == 0) {
        const char* vehicle_fields[] = {"totalCar", "totalBike", "totalBus", "totalTruck",
                                        "totalOtherVehicle"};
        for (guint i = 0; i < G_N_ELEMENTS(vehicle_fields); i++) {
            json_t* field_value = json_object_get(data, vehicle_fields[i]);
            if (json_is_integer(field_value)) {
                count += (gint)json_integer_value(field_value);
            }
        }
    } else {
        json_t* value = data ? json_object_get(data, config->category) : NULL;
        if (!json_is_integer(value)) {
            value = data ? json_object_get(data, "total") : NULL;
        }
        count = json_is_integer(value) ? (gint)json_integer_value(value) : 0;
    }
    const char* timestamp = data && json_is_string(json_object_get(data, "timeStamp"))
                                ? json_string_value(json_object_get(data, "timeStamp"))
                                : "";
    const char* reset_time = data && json_is_string(json_object_get(data, "resetTime"))
                                 ? json_string_value(json_object_get(data, "resetTime"))
                                 : "";

    pthread_mutex_lock(&app_state.mutex);
    app_state.count = count;
    state_replace(&app_state.label, config->label);
    state_replace(&app_state.scenario_uid, scenario_uid);
    state_replace(&app_state.scenario_name, scenario_name);
    state_replace(&app_state.scenario_type, scenario_type);
    state_replace(&app_state.category, config->category);
    state_replace(&app_state.timestamp, timestamp);
    state_replace(&app_state.reset_time, reset_time);
    state_replace(&app_state.api_version, api_version);
    state_replace(&app_state.last_error, "");
    pthread_mutex_unlock(&app_state.mutex);

    redraw_overlay();
    if (g_strcmp0(config->mode, "dynamic") == 0 || g_strcmp0(config->mode, "both") == 0) {
        update_dynamic_text(handle, credentials, config, count);
    }

    json_decref(response);
    g_free(scenario_uid);
    g_free(scenario_name);
    g_free(scenario_type);
    return TRUE;
}

static void* polling_thread(void* user_data) {
    (void)user_data;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* handle = curl_easy_init();
    if (!handle) {
        panic("Failed to initialize curl");
    }

    gchar* credentials = NULL;
    gchar* api_version = NULL;
    while (application_running) {
        AppConfig config = load_config();
        if (credentials == NULL) {
            g_free(credentials);
            credentials = build_runtime_credentials(&config);
            g_free(api_version);
            api_version = determine_api_version(handle, &config, credentials);
        }
        if (g_strcmp0(config.mode, "custom") == 0 || g_strcmp0(config.mode, "dynamic") == 0 ||
            g_strcmp0(config.mode, "both") == 0) {
            update_count_from_aoa(handle, &config, api_version, credentials);
        }
        gint interval_ms = CLAMP((gint)g_ascii_strtoll(config.poll_interval_ms, NULL, 10), 250, 10000);
        app_config_clear(&config);
        usleep((useconds_t)interval_ms * 1000U);
    }

    g_free(api_version);
    g_free(credentials);
    curl_easy_cleanup(handle);
    curl_global_cleanup();
    return NULL;
}

static json_t* build_status_json(void) {
    AppConfig config = load_config();
    json_t* root = json_object();
    json_t* config_json = json_pack("{s:s,s:s,s:s,s:s,s:s,s:s}",
                                    "Label",
                                    config.label,
                                    "Mode",
                                    config.mode,
                                    "DynamicTextSlot",
                                    config.dynamic_text_slot,
                                    "ScenarioUid",
                                    config.scenario_uid,
                                    "Category",
                                    config.category,
                                    "PollIntervalMs",
                                    config.poll_interval_ms);

    pthread_mutex_lock(&app_state.mutex);
    json_t* state_json = json_pack("{s:i,s:s,s:s,s:s,s:s,s:s,s:s,s:s}",
                                   "count",
                                   app_state.count,
                                   "scenario_uid",
                                   app_state.scenario_uid ? app_state.scenario_uid : "",
                                   "scenario_name",
                                   app_state.scenario_name ? app_state.scenario_name : "",
                                   "scenario_type",
                                   app_state.scenario_type ? app_state.scenario_type : "",
                                   "category",
                                   app_state.category ? app_state.category : "",
                                   "last_update",
                                   app_state.timestamp ? app_state.timestamp : "",
                                   "reset_time",
                                   app_state.reset_time ? app_state.reset_time : "",
                                   "last_error",
                                   app_state.last_error ? app_state.last_error : "");
    pthread_mutex_unlock(&app_state.mutex);

    CURL* handle = curl_easy_init();
    gchar* credentials = NULL;
    gchar* api_version = NULL;
    json_t* scenarios_json = json_array();
    if (handle) {
        credentials = build_runtime_credentials(&config);
        api_version = determine_api_version(handle, &config, credentials);
        json_decref(scenarios_json);
        scenarios_json = build_available_scenarios_json(handle, &config, api_version, credentials);
    }

    json_object_set_new(root, "config", config_json);
    json_object_set_new(root, "state", state_json);
    json_object_set_new(root, "available_scenarios", scenarios_json);
    g_free(api_version);
    g_free(credentials);
    if (handle) {
        curl_easy_cleanup(handle);
    }
    app_config_clear(&config);
    return root;
}

static int send_json_response(struct mg_connection* conn, int status_code, json_t* payload) {
    char* body = json_dumps(payload, JSON_INDENT(2));
    mg_printf(conn,
              "HTTP/1.1 %d OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
              status_code,
              strlen(body),
              body);
    free(body);
    return 1;
}

static gchar* read_request_body(struct mg_connection* conn) {
    char buffer[MAX_BODY_SIZE];
    int read = mg_read(conn, buffer, sizeof(buffer) - 1);
    if (read <= 0) {
        return g_strdup("");
    }
    buffer[read] = '\0';
    return g_strdup(buffer);
}

static int serve_static_file(struct mg_connection* conn, const char* path, const char* content_type) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        mg_printf(conn, "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* body = g_malloc((gsize)size + 1U);
    size_t bytes_read = fread(body, 1, (size_t)size, file);
    fclose(file);
    body[bytes_read] = '\0';
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n",
              content_type,
              (long)bytes_read);
    mg_write(conn, body, bytes_read);
    g_free(body);
    return 1;
}

static int handle_config(struct mg_connection* conn) {
    gchar* body = read_request_body(conn);
    json_error_t error;
    json_t* payload = json_loads(body, 0, &error);
    g_free(body);
    if (!payload) {
        json_t* response = json_pack("{s:s}", "error", error.text);
        int result = send_json_response(conn, 400, response);
        json_decref(response);
        return result;
    }

    const char* keys[] = {"Label", "Mode", "DynamicTextSlot", "ScenarioUid", "Category",
                          "PollIntervalMs"};
    for (guint i = 0; i < G_N_ELEMENTS(keys); i++) {
        json_t* value = json_object_get(payload, keys[i]);
        if (json_is_string(value)) {
            persist_config_value(keys[i], json_string_value(value));
        }
    }
    json_decref(payload);
    json_t* response = json_pack("{s:b}", "ok", 1);
    int result = send_json_response(conn, 200, response);
    json_decref(response);
    return result;
}

static int handle_reset_or_alarm(struct mg_connection* conn, const char* method_name) {
    AppConfig config = load_config();
    CURL* handle = curl_easy_init();
    gchar* credentials = build_runtime_credentials(&config);
    gchar* api_version = determine_api_version(handle, &config, credentials);
    gchar* scenario_uid = NULL;
    gchar* scenario_name = NULL;
    gchar* scenario_type = NULL;
    gboolean ok = discover_scenario(handle, &config, api_version, credentials, &scenario_uid,
                                    &scenario_name, &scenario_type);
    if (ok) {
        gchar* request = g_strdup_printf(
            "{\"apiVersion\":\"%s\",\"context\":\"big_aoa_counter\",\"method\":\"%s\",\"params\":{\"scenario\":%s}}",
            api_version,
            method_name,
            scenario_uid);
        char* response_text =
            vapix_post(handle, &config, "/local/objectanalytics/control.cgi", request, credentials);
        ok = response_text != NULL;
        g_free(response_text);
        g_free(request);
    }

    g_free(scenario_uid);
    g_free(scenario_name);
    g_free(scenario_type);
    g_free(api_version);
    g_free(credentials);
    curl_easy_cleanup(handle);
    app_config_clear(&config);

    json_t* response = json_pack("{s:b}", "ok", ok ? 1 : 0);
    int result = send_json_response(conn, ok ? 200 : 500, response);
    json_decref(response);
    return result;
}

static int handle_discover(struct mg_connection* conn) {
    persist_config_value("ScenarioUid", "");
    json_t* response = json_pack("{s:b}", "ok", 1);
    int result = send_json_response(conn, 200, response);
    json_decref(response);
    return result;
}

static int request_handler(struct mg_connection* conn, void* cb_data) {
    (void)cb_data;
    const struct mg_request_info* req = mg_get_request_info(conn);
    if (strcmp(req->request_uri, "/") == 0) {
        return serve_static_file(conn, "html/index.html", "text/html; charset=utf-8");
    }
    if (strcmp(req->request_uri, "/style.css") == 0) {
        return serve_static_file(conn, "html/style.css", "text/css; charset=utf-8");
    }
    if (strcmp(req->request_uri, "/app.js") == 0) {
        return serve_static_file(conn, "html/app.js", "application/javascript; charset=utf-8");
    }
    if (strcmp(req->request_uri, "/widget.html") == 0) {
        return serve_static_file(conn, "html/widget.html", "text/html; charset=utf-8");
    }
    if (strcmp(req->request_uri, "/widget.js") == 0) {
        return serve_static_file(conn, "html/widget.js", "application/javascript; charset=utf-8");
    }
    if (strcmp(req->request_uri, "/api/status") == 0) {
        json_t* response = build_status_json();
        int result = send_json_response(conn, 200, response);
        json_decref(response);
        return result;
    }
    if (strcmp(req->request_uri, "/api/config") == 0 && strcmp(req->request_method, "POST") == 0) {
        return handle_config(conn);
    }
    if (strcmp(req->request_uri, "/api/discover") == 0 && strcmp(req->request_method, "POST") == 0) {
        return handle_discover(conn);
    }
    if (strcmp(req->request_uri, "/api/reset") == 0 && strcmp(req->request_method, "POST") == 0) {
        return handle_reset_or_alarm(conn, "resetAccumulatedCounts");
    }
    if (strcmp(req->request_uri, "/api/send-alarm") == 0 && strcmp(req->request_method, "POST") == 0) {
        return handle_reset_or_alarm(conn, "sendAlarmEvent");
    }

    mg_printf(conn, "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
    return 1;
}

int main(void) {
    memset(&app_state, 0, sizeof(app_state));
    pthread_mutex_init(&app_state.mutex, NULL);
    openlog(APP_NAME, LOG_PID, LOG_USER);
    setenv("XDG_CACHE_HOME", "/usr/local/packages/" APP_NAME "/localdata", 1);

    GError* error = NULL;
    parameter_handle = ax_parameter_new(APP_NAME, &error);
    if (parameter_handle == NULL) {
        panic("Failed to init axparameter: %s", error->message);
    }

    signal(SIGTERM, stop_application);
    signal(SIGINT, stop_application);

    if (!axoverlay_is_backend_supported(AXOVERLAY_CAIRO_IMAGE_BACKEND)) {
        panic("AXOVERLAY_CAIRO_IMAGE_BACKEND is not supported");
    }

    struct axoverlay_settings settings;
    axoverlay_init_axoverlay_settings(&settings);
    settings.render_callback = draw_overlay;
    settings.adjustment_callback = adjustment_cb;
    settings.backend = AXOVERLAY_CAIRO_IMAGE_BACKEND;
    axoverlay_init(&settings, &error);
    if (error != NULL) {
        panic("Failed to initialize axoverlay: %s", error->message);
    }

    struct axoverlay_overlay_data overlay_data;
    setup_overlay_data(&overlay_data);
    overlay_data.width = 1280;
    overlay_data.height = 180;
    overlay_data.colorspace = AXOVERLAY_COLORSPACE_ARGB32;
    overlay_id = axoverlay_create_overlay(&overlay_data, NULL, &error);
    if (error != NULL) {
        panic("Failed to create overlay: %s", error->message);
    }
    redraw_overlay();

    const char* options[] = {"listening_ports", HTTP_PORT, "request_timeout_ms", "10000", 0};
    mg_init_library(0);
    web_context = mg_start(NULL, NULL, options);
    if (!web_context) {
        panic("Failed to start CivetWeb");
    }
    mg_set_request_handler(web_context, "/", request_handler, NULL);
    mg_set_request_handler(web_context, "/style.css", request_handler, NULL);
    mg_set_request_handler(web_context, "/app.js", request_handler, NULL);
    mg_set_request_handler(web_context, "/widget.html", request_handler, NULL);
    mg_set_request_handler(web_context, "/widget.js", request_handler, NULL);
    mg_set_request_handler(web_context, "/api/status", request_handler, NULL);
    mg_set_request_handler(web_context, "/api/config", request_handler, NULL);
    mg_set_request_handler(web_context, "/api/discover", request_handler, NULL);
    mg_set_request_handler(web_context, "/api/reset", request_handler, NULL);
    mg_set_request_handler(web_context, "/api/send-alarm", request_handler, NULL);

    pthread_t thread;
    pthread_create(&thread, NULL, polling_thread, NULL);

    main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGTERM, signal_handler, main_loop);
    g_unix_signal_add(SIGINT, signal_handler, main_loop);
    g_main_loop_run(main_loop);

    application_running = 0;
    pthread_join(thread, NULL);

    if (web_context) {
        mg_stop(web_context);
    }
    mg_exit_library();
    axoverlay_destroy_overlay(overlay_id, NULL);
    axoverlay_cleanup();
    ax_parameter_free(parameter_handle);
    g_main_loop_unref(main_loop);
    pthread_mutex_destroy(&app_state.mutex);
    g_free(app_state.label);
    g_free(app_state.scenario_uid);
    g_free(app_state.scenario_name);
    g_free(app_state.scenario_type);
    g_free(app_state.category);
    g_free(app_state.timestamp);
    g_free(app_state.reset_time);
    g_free(app_state.api_version);
    g_free(app_state.last_error);
    return 0;
}
