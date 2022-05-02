#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define SMS_TEXT_MAXLEN      160
#define SMS_PHONE_NUM_MAXLEN 12

extern xQueueHandle sms_config_queue;

typedef enum
{
    SIM800_NOERR = 0,
    SIM800_ERR
} sim800_err_t;

esp_err_t sim800_startUP(void);

#ifdef __cplusplus
}
#endif