#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_console.h"
#include "esp_bit_defs.h"

#include "sim800.h"
#include "console_commands.h"
#include "console_helper.h"

static const char *TAG = __FILE__;

static int console_cmd_handler(int argc, char **argv);

static const esp_console_cmd_t sim800_main_cmd = { .func     = console_cmd_handler,
                                                   .argtable = NULL,
                                                   .command  = "sim800",
                                                   .hint     = "<recepient number> <message>",
                                                   .help     = "dupa help sim800" };

QueueHandle_t g_sms_config_queue;

static esp_err_t
_send_sms(gsm_console_cmd_t *cmd, char *err)
{
    sms_config_t            config;
    gsm_console_cmd_param_t label;

    config.params_flags = cmd->params_flags;

    for (uint8_t params_qty = cmd->params_qty; params_qty > 0; params_qty--) {
        label = cmd->param[params_qty - 1].label;

        switch (label) {
        case GSM_CONSOLE_CMD_PARAM_NUMBER: {
            config.new_sms_number = cmd->param[params_qty - 1].param_val;
            config.params_flags |= BIT(SMS_CONFIG_NEW_NUMBER);
            ESP_LOGI(TAG, "new number for sms:\n%s", config.new_sms_number);
        } break;

        case GSM_CONSOLE_CMD_PARAM_MSG: {
            config.msg_text = cmd->param[params_qty - 1].param_val;
            config.params_flags |= BIT(SMS_CONFIG_NEW_MESSAGE);
            ESP_LOGI(TAG, "new message for sms:\n%s", config.msg_text);
        } break;

        default: {
            ESP_LOGE(TAG, "_send_sms: unknown label = %d", label);

            return ESP_ERR_INVALID_ARG;
        }
        }
    }

    BaseType_t queue_send_retval;

    queue_send_retval = xQueueSend(g_sms_config_queue, &config, 0);

    if (queue_send_retval == pdTRUE) {
        ESP_LOGI(TAG, "succesfully sent new sms config to queue");
    }
    else {
        ESP_LOGW(TAG, "no success during commitment of new sms config to queue");
    }

    return ESP_OK;
}

static esp_err_t
_execute_command(gsm_console_cmd_t *cmd, char *err)
{
    esp_err_t retval = ESP_OK;

    switch (cmd->name) {
    case GSM_CONSOLE_CMD_SEND_SMS: {
        retval = _send_sms(cmd, err);
        break;
    }

    default: strcpy(err, "_execute command -> default"); return ESP_OK;
    }

    if (retval != ESP_OK) {
        sprintf(err, "error occured during execution of command\nerror code is %d", retval);
    }

    return retval;
}

static void
_wait_untill_read_by_sms_task(void)
{
    UBaseType_t items_waiting = 1;

    while (items_waiting) {
        items_waiting = uxQueueMessagesWaiting(g_sms_config_queue);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static int
console_cmd_handler(int argc, char **argv)
{
    esp_err_t         retval;
    gsm_console_cmd_t cmd;
    char              err[CONFIG_GSM_CONSOLE_ERR_DESCR_LEN];

    argv++;
    argc--;

    retval = console_helper_decode_raw_command(argc, argv, &cmd, err);
    if (retval != ESP_OK) {
        char *error;

        error = 1 + strrchr(err, '\n');
        sprintf(error, "command failed, error code = %d\n", retval);

        ESP_LOGW(TAG, "%s", err);
        return GSM_CONSOLE_ERR1;
    }

    retval = _execute_command(&cmd, err);
    if (retval != ESP_OK) {
        char *error;

        error = 1 + strrchr(err, '\n');
        sprintf(error, "command failed, error code = %d\n", retval);

        ESP_LOGW(TAG, "%s", err);
        return GSM_CONSOLE_ERR1;
    }

    _wait_untill_read_by_sms_task();

    console_helper_free_memory(&cmd);

    return 0;
}

void
console_commands_init(void)
{
    console_helper_init();

    g_sms_config_queue = xQueueCreate(1, sizeof(sms_config_t));

    esp_console_cmd_register(&sim800_main_cmd);
}