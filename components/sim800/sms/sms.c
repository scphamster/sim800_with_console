#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "gsm.h"
#include "sim800.h"
#include "cmd_gsm.h"
#include "sms.h"

const char *TAG = "sms";

void
sms_prepare_defaults(cmd_gsm_queue_item_t *config)
{
    strcpy(config->msg.phone_number, CONFIG_GSM_SMS_NUMBER);
    strcpy(config->msg.text, "default text test");

    config->msg.phone_number[SMS_PHONE_NUM_MAXLEN] = '\0';
    config->msg.phone_number[SMS_TEXT_MAXLEN]      = '\0';
}

void
sms_prepare_to_use_sms(void)
{
    ppposDisconnect(0, 0);
    gsm_RFOn();   // Turn on RF if it was turned off
    vTaskDelay(2000 / portTICK_RATE_MS);
}

#if 0
esp_err_t
sms_check_new_config(char *number, char *text)
{
    BaseType_t retval;

    if (retval != pdTRUE) {
        return ESP_FAIL;
    }

    if (newconfig.flags & BIT(CMD_GSM_MSG)) {
        CMD_GSM_LOG(TAG,
                    "\nNew configurations obtained!\nnew sms number = %s\nmsg text is: %s",
                    newconfig.msg.phone_number,
                    newconfig.msg.text);
    }
    else if (newconfig.flags & BIT(CMD_GSM_RESPONSE_CFG)) {
        CMD_GSM_LOG(TAG,
                    "New configurations obtained!\nresponse type = %d\n message is: %s",
                    newconfig.response.resp_type,
                    newconfig.response.msg);
    }

    if (newconfig.flags & BIT(CMD_GSM_MSG)) {
        CMD_GSM_LOG(TAG, "copying msg params");

        strcpy(text, newconfig.msg.text);
        strcpy(number, newconfig.msg.phone_number);
    }

    CMD_GSM_LOG(TAG, "copied values are: number is: %s\nmsg is: %s", number, text);

    if (newconfig)) {
        CMD_GSM_LOG(TAG, "copying new response config");
    }

    return ESP_OK;
}
#endif

esp_err_t
sms_copy_config_data(cmd_gsm_queue_item_t *config, cmd_gsm_queue_item_t *newconfig)
{
    assert(config);
    assert(newconfig);

    if (newconfig->new_sms_send_config) {
        assert(newconfig->msg.text);
        assert(newconfig->msg.phone_number);

        strncpy(config->msg.text, newconfig->msg.text, SMS_TEXT_MAXLEN);
        strcpy(config->msg.phone_number, newconfig->msg.phone_number);
    }

    config->send_sms = newconfig->send_sms;
    config->read_sms = newconfig->read_sms;

    if (newconfig->new_response_config) {
        config->response.resp_type = newconfig->response.resp_type;
        strncpy(config->response.msg, newconfig->response.msg, SMS_TEXT_MAXLEN);
    }

    return ESP_OK;
}

esp_err_t
sms_send_sms(const char *sms_number, const char *sms_text)
{
    if (smsSend(sms_number, sms_text) == 1) {
        CMD_GSM_LOG(TAG, "SMS sent succesfully\n");

        return ESP_OK;
    }
    else {
        ESP_LOGW(TAG, "!SMS sending yielded no success!");

        return ESP_FAIL;
    }
}

esp_err_t
sms_handle_received_sms(GSM_sms *messages, cmd_gsm_queue_item_t *config)
{
    if (messages->nmsg == 0) {
        ESP_LOGI(TAG, "No SMS'es received");
        return ESP_ERR_NOT_FOUND;
    }

    char     buf[SMS_TEXT_MAXLEN + 1];
    char    *timestring;
    GSM_sms_msg *msg;

    ESP_LOGI(TAG, "Received messages number: %d", messages->nmsg);

    for (int i = 0; i < messages->nmsg; i++) {
        struct tm *timeinfo;

        msg = &(messages->messages[i]);

        timeinfo  = localtime(&msg->time_value);
        timestring = asctime(timeinfo);

        printf("-------------------------------------------\r\n");
        printf("\nMessage #%d: idx=%d, from: %s, status: %s, time: %s, tz=GMT+%d, timestamp:%s\n",
               i + 1,
               msg->idx,
               msg->from,
               msg->stat,
               msg->time,
               msg->tz,
               timestring);
        printf("Message text:\n%s\n", msg->msg);

        // Check if SMS text contains known command
        if (strstr(msg->msg, "Esp32 info") == msg->msg) {
            char   timestr[80];
            time_t rawtime;

            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(timestr, 80, "%x %H:%M:%S", timeinfo);
            sprintf(buf, "Hi, %s\rMy time is now\r%s", msg->from, timestr);

            if (smsSend(config->msg.phone_number, buf) == 1) {
                ESP_LOGI(TAG, "Response sent successfully\n");
            }
            else {
                ESP_LOGW(TAG, "Response send failed\n");
            }
        }

        // Free allocated message text buffer
        if (msg->msg)
            free(msg->msg);

        if ((i + 1) == messages->nmsg) {
            printf("Delete message at index %d\r\n", msg->idx);

            if (smsDelete(msg->idx) == 0)
                printf("Delete ERROR\r\n");
            else
                printf("Delete OK\r\n");
        }
    }

    free(messages->messages);

    return ESP_OK;
}