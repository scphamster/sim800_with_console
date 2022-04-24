#pragma once

#include <stdint.h>
#define strgze(str) #str

typedef enum
{
    GSM_CONSOLE_CMD_SEND_SMS = 0,
    GSM_CONSOLE_CMD_SET_NUMBER,
    GSM_CONSOLE_CMD_SET_MESSAGE,
    GSM_CONSOLE_CMD_SET_AUTORESPONSE,
    GSM_CONSOLE_CMD_NAME_ERR,

    GSM_CONSOLE_CMD_MAX
} gsm_console_cmd_name_t;

typedef enum
{
    GSM_CONSOLE_CMD_PARAM_NUMBER = 0,
    GSM_CONSOLE_CMD_PARAM_MSG,

    GSM_CONSOLE_CMD_PARAM_MAX
} gsm_console_cmd_param_t;

typedef struct {
    gsm_console_cmd_param_t label;
    char                   *param_val;
} gsm_console_cmd_param_val_pair_t;

typedef struct {
    gsm_console_cmd_name_t            name;
    gsm_console_cmd_param_val_pair_t *param;
    uint8_t                           params_flags;
    uint8_t                           params_qty;
} gsm_console_cmd_t;

void      console_helper_init(void);
esp_err_t console_helper_free_memory(gsm_console_cmd_t *cmd);
esp_err_t console_helper_decode_raw_command(
  int argc, char **argv, gsm_console_cmd_t *cmd, char *err);