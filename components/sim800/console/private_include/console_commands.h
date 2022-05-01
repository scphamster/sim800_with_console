#pragma once
#define CONFIG_GSM_CONSOLE_ERR_DESCR_LEN 300

typedef enum
{
    GSM_CONSOLE_NOERR = 0,
    GSM_CONSOLE_ERR1,

    GSM_CONSOLE_ERR_MAX
} gsm_console_err_t;

typedef enum
{
    SMS_CONFIG_NEW_NUMBER = 0,
    SMS_CONFIG_NEW_MESSAGE
} sms_config_param_flag_t;

typedef struct {
    bool  new_number_is_set   :1;
    bool  new_msg_text_is_set :1;
    char *new_number;
    char *new_msg;
} sim800_console_cmds_t;

typedef struct {
    char   *new_sms_number;
    char   *msg_text;
    uint8_t params_flags;
} sms_config_t;

typedef struct {
    const char *signature;
    const char *help;
    const char *hint;
} sim800_console_cmd_descriptor_t;

extern QueueHandle_t g_sms_config_queue;

void console_commands_init(void);