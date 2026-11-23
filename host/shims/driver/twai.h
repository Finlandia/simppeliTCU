// ESP-IDF TWAI (CAN) driver API replacement. The host backend (twai_host.cpp)
// routes frames to Linux SocketCAN, to a candump log replay, or nowhere.
#pragma once

#include <cstdint>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x107

typedef int gpio_num_t;
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

typedef enum { TWAI_MODE_NORMAL, TWAI_MODE_NO_ACK, TWAI_MODE_LISTEN_ONLY } twai_mode_t;

typedef struct {
    union {
        struct {
            uint32_t extd : 1;
            uint32_t rtr : 1;
            uint32_t ss : 1;
            uint32_t self : 1;
            uint32_t dlc_non_comp : 1;
            uint32_t reserved : 27;
        };
        uint32_t flags;
    };
    uint32_t identifier;
    uint8_t data_length_code;
    uint8_t data[8];
} twai_message_t;

typedef struct {
    gpio_num_t tx_io;
    gpio_num_t rx_io;
    twai_mode_t mode;
    uint32_t tx_queue_len;
    uint32_t rx_queue_len;
} twai_general_config_t;

typedef enum {
    TWAI_STATE_STOPPED,
    TWAI_STATE_RUNNING,
    TWAI_STATE_BUS_OFF,
    TWAI_STATE_RECOVERING,
} twai_state_t;

// Same fields as ESP-IDF's twai_status_info_t
typedef struct {
    twai_state_t state;
    uint32_t msgs_to_tx;
    uint32_t msgs_to_rx;
    uint32_t tx_error_counter;
    uint32_t rx_error_counter;
    uint32_t tx_failed_count;
    uint32_t rx_missed_count;
    uint32_t rx_overrun_count;
    uint32_t arb_lost_count;
    uint32_t bus_error_count;
} twai_status_info_t;

typedef struct {
    uint32_t bitrate;
} twai_timing_config_t;

typedef struct {
    uint32_t acceptance_code;
    uint32_t acceptance_mask;
    bool single_filter;
} twai_filter_config_t;

#define TWAI_TIMING_CONFIG_500KBITS() {500000}
#define TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, m) {(tx), (rx), (m), 5, 5}
#define TWAI_FILTER_CONFIG_ACCEPT_ALL() {0, 0xFFFFFFFF, true}

esp_err_t twai_driver_install(const twai_general_config_t* g, const twai_timing_config_t* t,
                              const twai_filter_config_t* f);
esp_err_t twai_start();
esp_err_t twai_transmit(const twai_message_t* message, TickType_t ticks);
esp_err_t twai_receive(twai_message_t* message, TickType_t ticks);
esp_err_t twai_get_status_info(twai_status_info_t* status_info);
