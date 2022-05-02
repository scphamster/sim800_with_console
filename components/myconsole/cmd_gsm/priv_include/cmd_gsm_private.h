#pragma once

#include "argtable3/argtable3.h"

#define SMS_SEND_CMD_NUMBER_MINCOUNT  0
#define SMS_SEND_CMD_NUMBER_MAXCOUNT  1
#define SEND_SMS_CMD_MESSAGE_MINCOUNT 0
#define SEND_SMS_CMD_MESSAGE_MAXCOUNT 1
#define SEND_SMS_CMD_ARGUMENT_NUM     4

#define ERRBUF_LEN  200

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct arg_str *number;
    struct arg_str *message;
    struct arg_lit *send;
    struct arg_lit *read;
    struct arg_end *end;
} send_sms_cmd_args_t;

#ifdef __cpluplus
}
#endif