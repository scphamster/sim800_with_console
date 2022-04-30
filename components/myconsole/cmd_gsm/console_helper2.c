#include "sdkconfig.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_bit_defs.h"

#include "cmd_gsm.h"
#include "cmd_gsm_private.h"

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

    ESP_LOGI(TAG, "reallocating");
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

static esp_err_t
check_number__append_plus_sign(const char **number)
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
static esp_err_t
check_number(const char **number)
{
    uint8_t   n_chars;
    esp_err_t retval;

    // no '+' sign before the number, so we have to append number with it
    // allocate new memmory for string with + sign at front
    if (*number[0] != '+') {
#if (CONFIG_GSM_CONSOLE_DEBUG == true)
        ESP_LOGI(TAG,
                 "number received == %s has no + sign at front. addr pointed by *number is %p",
                 *number,
                 *number);
#endif
        retval = check_number__append_plus_sign(number);
        if (retval != ESP_OK) {
            ESP_LOGE(TAG, "no succes in appending + sign to number");

            return retval;
        }

        ESP_LOGI(TAG, "addr pointer by *number is %p", *number);
    }

    n_chars = strchr(*number, '\0') - *number;
    if (n_chars == 12) {
#if (CONFIG_GSM_CONSOLE_DEBUG == true)
        ESP_LOGI(TAG, "%s has %u n_chars", *number, n_chars);
#endif
    }
    else {
        ESP_LOGE(TAG, "number has %u characters", n_chars);

        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/**
 * @brief should be invoked after send_sms_cmd_check_args(), when cmd struct is not needed anymore
 *
 */
static void
free_allocated_buffers(void)
{
    for (int i = 0; i < n_allocated_items; i++) {
        free((void *)buffers[i]);
    }

    free((void *)buffers);

    buffers           = NULL;
    n_allocated_items = 0;
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
send_sms_cmd_check_args(send_sms_cmd_args_t *cmd, char *errbuf)
{
    if (cmd->number->count) {
        uint8_t      count = cmd->number->count;
        const char **str   = cmd->number->sval;

        while (count--) {
#if (CONFIG_GSM_CONSOLE_DEBUG == true)
            ESP_LOGI(TAG, "number No %d is %s\n", count, *str);
#endif   //(CONFIG_GSM_CONSOLE_DEBUG == true)
            check_number(str);
            str++;
        }
    }

    if (cmd->message->count) {
        uint8_t      count = cmd->message->count;
        const char **str   = cmd->message->sval;

        while (count--) {
            ESP_LOGI(TAG, "message is %s\n", *str);

            str++;
        }
    }

    return ESP_OK;
}