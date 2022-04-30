#pragma once

#include "esp_err.h"
#include "cmd_gsm_private.h"

/**
 * @brief should be invoked after send_sms_cmd_check_args(), when cmd struct is not needed anymore
 *
 */
static void free_allocated_buffers(void);
/**
 * @brief checks all arguments in cmd for correctnes. fixes some issues
 *
 *
 * @param cmd struct with params inserted by arg_parse(argc, argv, (void **)&Sms_args)
 * @param errbuf char[] buffer for errors to be inserted
 * errors are being inserted begining from first function that encountered error with '\n' sign at
 * the end of its input
 * @return esp_err_t error code or ESP_OK
 */
esp_err_t   send_sms_cmd_check_args(send_sms_cmd_args_t *cmd, char *errbuf);