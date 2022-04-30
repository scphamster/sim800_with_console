#pragma once

#define CMD_GSM_NUMBER_CHAR_QTY 12   // '+', country code - 2, number - 9.

typedef enum
{
    GSM_MESSAGE = 0,
    GSM_NUMBER,
    GSM_REPEATER_CONFIG
} console_to_gsm_msg_flags_t;

typedef struct {
    char   *text_of_messge;
    char   *phone_number;
    uint8_t flags;
} console_to_gsm_msg_t;

void register_send_sms(void);