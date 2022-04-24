#pragma once

#include "esp_err.h"
#include "freertos/queue.h"

typedef enum { SIM800_NOERR = 0, SIM800_ERR } sim800_err_t;

esp_err_t sim800_startUP(void);
