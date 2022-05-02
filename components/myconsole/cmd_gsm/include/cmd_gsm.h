#pragma once

#include "sdkconfig.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_GSM_NUMBER_CHAR_QTY 12   // '+', country code - 2, number - 9.

#if (CONFIG_GSM_DEBUG == 1)
#define CMD_GSM_LOG(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#else
#define CMD_GSM_LOG(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#endif

typedef enum
{
    CMD_GSM_RESPONSE1 = 0,
    CMD_GSM_RESPONSE2
} cmd_gsm_response_type_t;

typedef struct {
    char *text;
    char *phone_number;
} cmd_gsm_msg_t;

typedef struct {
    cmd_gsm_response_type_t resp_type;
    char                   *msg;
} cmd_gsm_response_cfg_t;

typedef struct {
    bool send_sms            :1;
    bool new_sms_send_config :1;
    bool read_sms            :1;
    bool new_response_config :1;

    cmd_gsm_msg_t          msg;
    cmd_gsm_response_cfg_t response;
} cmd_gsm_queue_item_t;

void register_gsm(void);

#ifdef __cplusplus
}
#endif