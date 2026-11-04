#pragma once
#include <stddef.h>
#include "esp_err.h"
#include <stdint.h>
typedef struct { uint8_t* src_addr; } esp_now_recv_info_t;
typedef struct { uint8_t peer_addr[6]; int channel; int ifidx; int encrypt; } esp_now_peer_info_t;
typedef void (*esp_now_recv_cb_t)(const esp_now_recv_info_t* info, const uint8_t* data, int len);
esp_err_t esp_now_init(void);
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t cb);
esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer);
esp_err_t esp_now_send(const uint8_t* addr, const uint8_t* data, size_t len);
