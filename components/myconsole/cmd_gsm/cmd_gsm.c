#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include "esp_err.h"

#include "driver/rtc_io.h"
#include "driver/uart.h"

#include "argtable3/argtable3.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "cmd_gsm_private.h"

#include "sim800.h"
#include "console_helper2.h"

static const char         *TAG = "CMD_GSM";
static send_sms_cmd_args_t Sms_args;

static int
send_sms(int argc, char **argv)
{
    int       nerrors;
    esp_err_t retval;
    char      errbuf[ERRBUF_LEN];

    nerrors = arg_parse(argc, argv, (void **)&Sms_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, Sms_args.end, argv[0]);

        return 1;
    }

    retval = send_sms_cmd_check_args(&Sms_args, errbuf);
    if (retval != ESP_OK) {
        ESP_LOGW(TAG,
                 "send_sms command not succeed.\nsend_sms_cmd_check_args error code is %d\n%s",
                 retval,
                 errbuf);
    }

    ESP_LOGI(TAG, "send sms command invoked");
    
    free_allocated_buffers();
    return 0;
}

void
register_send_sms(void)
{
    Sms_args.number = arg_strn("-n",
                                    "--number",
                                    "<string>",
                                    SMS_SEND_CMD_NUMBER_MINCOUNT,
                                    SMS_SEND_CMD_NUMBER_MAXCOUNT,
                                    "recipient phone number");

    Sms_args.message = arg_strn("-m",
                                     "--message",
                                     "<string>",
                                     SEND_SMS_CMD_MESSAGE_MINCOUNT,
                                     SEND_SMS_CMD_MESSAGE_MAXCOUNT,
                                     "message for recipient");

    Sms_args.end = arg_end(SEND_SMS_CMD_ARGUMENT_NUM);

    const esp_console_cmd_t cmd = { .command  = "send_sms",
                                    .help     = "Send sms to desired phone number. ",
                                    .hint     = "<number> <message>",
                                    .func     = send_sms,
                                    .argtable = &Sms_args };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}