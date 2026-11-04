#pragma once
#include "esp_err.h"
typedef struct fake_http_client* esp_http_client_handle_t;
typedef enum { HTTP_METHOD_GET, HTTP_METHOD_POST } esp_http_client_method_t;
typedef enum { HTTP_EVENT_ON_CONNECTED, HTTP_EVENT_ON_DATA } esp_http_client_event_id_t;
typedef struct { esp_http_client_event_id_t event_id; void* data; int data_len; } esp_http_client_event_t;
typedef esp_err_t (*http_event_handle_cb)(esp_http_client_event_t* event);
typedef struct { const char* url; esp_http_client_method_t method; int timeout_ms; http_event_handle_cb event_handler; } esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* config);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char* key, const char* value);
esp_err_t esp_http_client_set_url(esp_http_client_handle_t client, const char* url);
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char* data, int len);
esp_err_t esp_http_client_perform(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
esp_err_t esp_http_client_close(esp_http_client_handle_t client);
