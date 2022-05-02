#include "sdkconfig.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_bit_defs.h"

#include "sim800.h"
#include "cmd_gsm.h"
#include "cmd_gsm_private.h"
#include "cmd_parser.h"

static const char  *TAG               = "console helper";
static uint32_t     n_allocated_items = 0;
static const char **buffers           = NULL;

/**
 * @brief allocate memmory for modified string in command arguments struct
 *
 * @param str_len length of string (without null character)
 * @param ret_buffer *ret_buffer - points to allocated memmory location
 * @return esp_err_t ESP_OK, ESP_ERR_NO_MEM if no success on allocation of new memmory,
 * ESP_ERR_INVALID_ARG - if str_len is <= 0.
 */
static esp_err_t
alloc_new_string_buf(int str_len, char **ret_buffer)
{
    char *newbuf;

    if (str_len <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    newbuf = (char *)malloc(sizeof(char) * (str_len + 1));
    if (newbuf == NULL) {
        ESP_LOGE(TAG,
                 "%s %d\nallocation of memmory for new buffer unsuccessful",
                 __FILE__,
                 __LINE__);

        return ESP_ERR_NO_MEM;
    }

    buffers = (const char **)realloc(buffers, sizeof(char *) * (n_allocated_items + 1));
    if (buffers == NULL) {
        ESP_LOGE(TAG,
                 "%s %d\nallocation of memmory for new string table unsuccessful",
                 __FILE__,
                 __LINE__);
        return ESP_ERR_NO_MEM;
    }

    buffers[n_allocated_items] = newbuf;
    *ret_buffer                = newbuf;

    n_allocated_items++;

    return ESP_OK;
}

static inline esp_err_t
_check_number__append_plus_sign(const char **number)
{
    char     *newbuf;
    esp_err_t retval;

    retval = alloc_new_string_buf(CMD_GSM_NUMBER_CHAR_QTY, &newbuf);
    if (retval != ESP_OK) {
        const char *err_str;

        err_str = esp_err_to_name(retval);
        ESP_LOGE(TAG, "unsuccessful allocation\nerror: %s\n %s %d\n", err_str, __FILE__, __LINE__);
        return retval;
    }

    newbuf[0] = '+';

    strcpy((newbuf + 1), *number);

    *number = newbuf;

    return ESP_OK;
}

/**
 * @brief checks if number has correct format, some automatic corrections may be undertaken
 *
 * @param number pointer to pointer of number string
 * @return esp_err_t ESP_OK or ESP_ERR_NO_MEM
 */
static inline esp_err_t
_check_args__check_number(const char **number)
{
    uint8_t   n_chars;
    esp_err_t retval;

    // no '+' sign before the number, so we have to append number with it
    // allocate new memmory for string with + sign at front
    if (*number[0] != '+') {
        CMD_GSM_LOG(TAG, "number received == %s has no + sign at front. Appending", *number);

        retval = _check_number__append_plus_sign(number);
        if (retval != ESP_OK) {
            ESP_LOGE(TAG, "no succes in appending + sign to number");

            return retval;
        }

        CMD_GSM_LOG(TAG,
                    "Success! New number is %s",
                    *number);
    }

    n_chars = strchr(*number, '\0') - *number;
    if (n_chars != 12) {
        ESP_LOGE(TAG, "Number has %u characters. Should have 12 characters", n_chars);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/**
 * @brief should be invoked after send_sms__check_args(), when cmd struct is not needed anymore
 *
 */
void
free_allocated_buffers(void)
{
    for (int i = 0; i < n_allocated_items; i++) {
        free((void *)buffers[i]);
    }

    free((void *)buffers);

    buffers           = NULL;
    n_allocated_items = 0;

    CMD_GSM_LOG(TAG, "Allocated buffers have been liberated");
}

/**
 * @brief checks all arguments in cmd for correctnes. fixes some issues
 * @note free_allocated_buffers should be invoked later, when all cmd struct a
 *
 * @param cmd struct with params inserted by arg_parse(argc, argv, (void **)&Sms_args)
 * @param errbuf char[] buffer for errors to be inserted
 * errors are being inserted begining from first function that encountered error with '\n' sign at
 * the end of its input
 * @return esp_err_t error code or ESP_OK
 */
esp_err_t
send_sms__check_args(send_sms_cmd_args_t *cmd, char *errbuf)
{
    CMD_GSM_LOG(TAG, "processing arguments...");
    
    if (cmd->number->count) {
        uint8_t      count = cmd->number->count;
        const char **str   = cmd->number->sval;

        while (count--) {
            CMD_GSM_LOG(TAG, "number No %d is %s", count, *str);

            _check_args__check_number(str++);
        }
    }

    if (cmd->message->count) {
        uint8_t      count = cmd->message->count;
        const char **str   = cmd->message->sval;

        while (count--) {
            CMD_GSM_LOG(TAG, "message is: %s", *str);

            str++;
        }
    }

    CMD_GSM_LOG(TAG, "all arguments have been processed");

    return ESP_OK;
}

static void
_post_data__prepare_data(cmd_gsm_queue_item_t *sms_config, send_sms_cmd_args_t *cmd)
{
    if (cmd->send->count) {
        sms_config->send_sms = true;
        CMD_GSM_LOG(TAG, "Sending to queue: send sms");
    }
    
    if (cmd->message->count) {
        sms_config->msg.text = cmd->message->sval[0];
        sms_config->new_sms_send_config = true;
        CMD_GSM_LOG(TAG, "Sending to queue: message: %s", sms_config->msg.text);
    }

    if (cmd->number->count) {
        sms_config->msg.phone_number  = cmd->number->sval[0];
        sms_config->new_sms_send_config = true;
        CMD_GSM_LOG(TAG, "Sending to queue: number: %s", sms_config->msg.phone_number);
    }

    sms_config->read_sms = true;
    
    sms_config->response.msg       = NULL;
    sms_config->response.resp_type = 0;
    sms_config->new_response_config = false;
}

static void
_post_data__wait_for_reception(void)
{
    UBaseType_t items_waiting = 1;

    while (items_waiting) {
        items_waiting = uxQueueMessagesWaiting(sms_config_queue);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief prepare and send data from cmd to sms queue
 * @note cmd struct should be already processed by send_sms__check_args()
 * @param cmd struct with params inserted by arg_parse(argc, argv, (void **)&Sms_args)
 * @param errbuf char[] buffer for errors to be inserted
 * errors are being inserted begining from first function that encountered error with '\n' sign at
 * the end of its input
 * @return esp_err_t ESP_OK on success or error code on fail
 */
esp_err_t
send_sms__post_data(send_sms_cmd_args_t *cmd, char *errbuf)
{
    BaseType_t           qretval;
    cmd_gsm_queue_item_t sms_config;

    _post_data__prepare_data(&sms_config, cmd);

    qretval = xQueueSend(sms_config_queue, &sms_config, 0);

    if (qretval == pdTRUE) {
        CMD_GSM_LOG(TAG, "new sms config has been posted to queue");
    }
    else {
        CMD_GSM_LOG(TAG, "queue is full, unsuccessful dispatch");
    }

    _post_data__wait_for_reception();

    return ESP_OK;
}