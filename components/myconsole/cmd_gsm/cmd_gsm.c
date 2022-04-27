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

#include "driver/rtc_io.h"
#include "driver/uart.h"

#include "argtable3/argtable3.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cmd_gsm_private.h"

static const char *TAG = "CMD_GSM";

static struct {
    struct arg_str *number;
    struct arg_str *message;
    struct arg_end *end;

} Send_sms_args;

static int
send_sms(int argc, char **argv)
{
    int nerrors;

    nerrors = arg_parse(argc, argv, (void **)&Send_sms_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, Send_sms_args.end, argv[0]);
        return 1;
    }
    
    
    
    ESP_LOGI(TAG, "send sms command invoked");

    return 0;
}

void
register_send_sms(void)
{
    Send_sms_args.number  = arg_strn("-n",
                                    "--number",
                                    "<string>",
                                    SMS_SEND_CMD_NUMBER_MINCOUNT,
                                    SMS_SEND_CMD_NUMBER_MAXCOUNT,
                                    "recipient phone number");

    Send_sms_args.message = arg_strn("-m",
                                     "--message",
                                     "<string>",
                                     SEND_SMS_CMD_MESSAGE_MINCOUNT,
                                     SEND_SMS_CMD_MESSAGE_MAXCOUNT,
                                     "message for recipient");

    Send_sms_args.end = arg_end(SEND_SMS_CMD_ARGUMENT_NUM);

    const esp_console_cmd_t cmd = { .command  = "send_sms",
                                    .help     = "Send sms to desired phone number. ",
                                    .hint     = "<number> <message>",
                                    .func     = send_sms,
                                    .argtable = &Send_sms_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));                          
}