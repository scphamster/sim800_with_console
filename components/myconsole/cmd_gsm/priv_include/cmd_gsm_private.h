#pragma once

#include "argtable3/argtable3.h"

#define SMS_SEND_CMD_NUMBER_MINCOUNT  0
#define SMS_SEND_CMD_NUMBER_MAXCOUNT  15   //"+CC123456789" => 13 symbols (\0 included) + 2 safety
#define SEND_SMS_CMD_MESSAGE_MINCOUNT 0
#define SEND_SMS_CMD_MESSAGE_MAXCOUNT 50
#define SEND_SMS_CMD_ARGUMENT_NUM     2

#define ERRBUF_LEN  200

typedef struct {
    struct arg_str *number;
    struct arg_str *message;
    struct arg_end *end;
} send_sms_cmd_args_t;