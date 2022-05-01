#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_bit_defs.h"

#include "console_helper.h"

const char *TAG = "HELPER";

const char *cmd_send_sms         = "sms";
const char *cmd_set_number       = "number";
const char *cmd_set_message      = "message";
const char *cmd_set_autoresponse = "autoresponse";
const char *commands[GSM_CONSOLE_CMD_MAX];

const char *cmd_sms_param_set_number  = "-n";
const char *cmd_sms_param_set_message = "-m";
const char *cmd_sms_params[GSM_CONSOLE_CMD_PARAM_MAX];

void
console_helper_init(void)
{
    commands[GSM_CONSOLE_CMD_SEND_SMS]         = cmd_send_sms;
    commands[GSM_CONSOLE_CMD_SET_NUMBER]       = cmd_set_number;
    commands[GSM_CONSOLE_CMD_SET_MESSAGE]      = cmd_set_message;
    commands[GSM_CONSOLE_CMD_SET_AUTORESPONSE] = cmd_set_autoresponse;

    cmd_sms_params[GSM_CONSOLE_CMD_PARAM_NUMBER] = cmd_sms_param_set_number;
    cmd_sms_params[GSM_CONSOLE_CMD_PARAM_MSG]    = cmd_sms_param_set_message;
}

static inline esp_err_t
_find_cmd_name(char **argv, gsm_console_cmd_t *cmd, char *err)
{
    ESP_LOGI(TAG, "*argv = %s", *argv);
    
    if (strstr(*argv, commands[GSM_CONSOLE_CMD_SEND_SMS]) == *argv) {
        cmd->name = GSM_CONSOLE_CMD_SEND_SMS;

        ESP_LOGI(TAG, "name of command is %s", strgze(GSM_CONSOLE_CMD_SEND_SMS));

        return ESP_OK;
    }
    else if (strstr(*argv, commands[GSM_CONSOLE_CMD_SET_NUMBER]) == *argv) {
        cmd->name = GSM_CONSOLE_CMD_SET_NUMBER;

        return ESP_OK;
    }
    else if (strstr(*argv, commands[GSM_CONSOLE_CMD_SET_MESSAGE]) == *argv) {
        cmd->name = GSM_CONSOLE_CMD_SET_MESSAGE;

        return ESP_OK;
    }
    else if (strstr(*argv, commands[GSM_CONSOLE_CMD_SET_AUTORESPONSE]) == *argv) {
        cmd->name = GSM_CONSOLE_CMD_SET_AUTORESPONSE;

        return ESP_OK;
    }
    else {
        cmd->name = GSM_CONSOLE_CMD_NAME_ERR;
        strcpy(err, "find name -> unknown command name\n");

        return ESP_ERR_INVALID_ARG;
    }
}

static inline esp_err_t
_allocate_memory_for_params(gsm_console_cmd_t *cmd)
{
    gsm_console_cmd_param_val_pair_t *param_new_storage = NULL;

    param_new_storage = (gsm_console_cmd_param_val_pair_t *)realloc(
      cmd->param,
      cmd->params_qty * sizeof(gsm_console_cmd_param_val_pair_t));

    if (param_new_storage) {
        cmd->param = param_new_storage;
        return ESP_OK;
    }
    else {
        return ESP_ERR_NO_MEM;
    }
}

static inline esp_err_t
_find_params_cmd_send_sms(int argc, char **argv, gsm_console_cmd_t *cmd, char *err)
{
    esp_err_t retval;

    if (strstr(*argv, cmd_sms_params[GSM_CONSOLE_CMD_PARAM_NUMBER]) == *argv) {
        cmd->params_qty++;

        argv++;
        argc--;

        retval = _allocate_memory_for_params(cmd);
        if (retval != ESP_OK) {
            sprintf(
              err,
              "error ocured during (re)allocating memory for command parameter %s\n error code == "
              "%d\n",
              strgze(GSM_CONSOLE_CMD_PARAM_NUMBER),
              retval);

            return ESP_ERR_NO_MEM;
        }

        if (argc <= 0) {
            cmd->param[cmd->params_qty - 1].param_val = NULL;

            sprintf(
              err,
              "no value found in input buffer for parameter:\n%s\n",
              strgze(GSM_CONSOLE_CMD_PARAM_NUMBER));

            return ESP_ERR_NOT_FOUND;
        }
        else {
            cmd->param[cmd->params_qty - 1].label     = GSM_CONSOLE_CMD_PARAM_NUMBER;
            cmd->param[cmd->params_qty - 1].param_val = *argv;
            cmd->params_flags |= BIT(GSM_CONSOLE_CMD_PARAM_NUMBER);
        }

        return ESP_OK;
    }
    else if (strstr(*argv, cmd_sms_params[GSM_CONSOLE_CMD_PARAM_MSG]) == *argv) {
        cmd->params_qty++;
        argv++;
        argc--;

        retval = _allocate_memory_for_params(cmd);
        if (retval != ESP_OK) {
            sprintf(
              err,
              "error ocured during (re)allocating memory for command parameter %s\n error code == "
              "%d\n",
              strgze(GSM_CONSOLE_CMD_PARAM_MSG),
              retval);

            return ESP_ERR_NO_MEM;
        }

        if (argc <= 0) {
            cmd->param[cmd->params_qty - 1].param_val = NULL;

            sprintf(
              err,
              "no value found in input buffer for parameter:\n%s\n",
              strgze(GSM_CONSOLE_CMD_PARAM_MSG));

            return ESP_ERR_NOT_FOUND;
        }
        else {
            cmd->param[cmd->params_qty - 1].label     = GSM_CONSOLE_CMD_PARAM_MSG;
            cmd->param[cmd->params_qty - 1].param_val = *argv;
            cmd->params_flags |= BIT(GSM_CONSOLE_CMD_PARAM_MSG);
        }

        return ESP_OK;
    }
    else {
        sprintf(err, "not found any registered parameters for sms command\n");

        return ESP_ERR_NOT_FOUND;
    }
}

static esp_err_t
_find_cmd_params(int argc, char **argv, gsm_console_cmd_t *cmd, char *err)
{
    if (argc <= 0) {
        cmd->params_qty = 0;
        cmd->param      = NULL;

        return ESP_OK;
    }

    esp_err_t retval;
    cmd->param        = NULL;
    cmd->params_flags = 0;
    cmd->params_qty   = 0;

    ESP_LOGI(TAG, "performing finding of parameters...\nargc = %d, *argv = %s", argc, *argv);

    while (argc > 0) {
        switch (cmd->name) {
        case GSM_CONSOLE_CMD_SEND_SMS: {
            retval = _find_params_cmd_send_sms(argc, argv, cmd, err);

            if (retval != ESP_OK) {
                err = 1 + strrchr(err, '\n');

                sprintf(
                  err,
                  "failed with finding parameters from input\nerror code = %d\n",
                  retval);

                return ESP_ERR_INVALID_RESPONSE;
            }
        } break;

        case GSM_CONSOLE_CMD_SET_NUMBER: {
            return ESP_ERR_INVALID_ARG;
        } break;

        case GSM_CONSOLE_CMD_SET_MESSAGE: {
            return ESP_ERR_INVALID_ARG;
        } break;

        case GSM_CONSOLE_CMD_SET_AUTORESPONSE: {
            return ESP_ERR_INVALID_ARG;
        } break;

        default: {
            return ESP_ERR_INVALID_ARG;
        }
        }

        argc -= 2;
        argv += 2;
    }

    return ESP_OK;
}

esp_err_t
console_helper_free_memory(gsm_console_cmd_t *cmd)
{
    ESP_LOGI(TAG, "clearing memory");
    
    if (cmd->params_qty == 0) {
        return ESP_OK;
    }

    free(cmd->param);

    return ESP_OK;
}

esp_err_t
console_helper_decode_raw_command(int argc, char **argv, gsm_console_cmd_t *cmd, char *err)
{
    esp_err_t retval;

    retval = _find_cmd_name(argv, cmd, err);
    if (retval != ESP_OK) {
        err = 1 + strrchr(err, '\n');

        sprintf(err, "input decode failed at finding name of command, error code = %d\n", retval);

        return ESP_ERR_INVALID_RESPONSE;
    }

    argv++;
    argc--;

    retval = _find_cmd_params(argc, argv, cmd, err);
    if (retval != ESP_OK) {
        err = 1 + strrchr(err, '\n');

        sprintf(err, "finding parameters failed, error code = %d\n", retval);

        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}
