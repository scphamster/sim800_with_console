
#define GSM_TEST_PPP_INCLUDE

#include "sdkconfig.h"
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_console.h"

#include "sim800.h"
#include "console_main.h"

static const char *TAG = __FILE__;

void
app_main(void)
{
    ESP_LOGI(TAG, "creating console task");

    //ESP_LOGI(TAG, "this is message from test branch");

    xTaskCreatePinnedToCore(console_task, "console", 8000, NULL, 1, NULL, tskNO_AFFINITY);
    
    sim800_startUP();
}