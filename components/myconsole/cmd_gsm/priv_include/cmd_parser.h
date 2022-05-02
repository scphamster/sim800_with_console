#pragma once

#include "cmd_gsm_private.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief should be invoked after send_sms__check_args(), when cmd struct is not needed anymore
 *
 */
void free_allocated_buffers(void);
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
esp_err_t send_sms__check_args(send_sms_cmd_args_t *cmd, char *errbuf);

/**
 * @brief prepare and send data from cmd to sms queue
 * @note cmd struct should be already processed by send_sms__check_args()
 * @param cmd struct with params inserted by arg_parse(argc, argv, (void **)&Sms_args)
 * @param errbuf char[] buffer for errors to be inserted
 * errors are being inserted begining from first function that encountered error with '\n' sign at
 * the end of its input
 * @return esp_err_t ESP_OK on success or error code on fail
 */
esp_err_t send_sms__post_data(send_sms_cmd_args_t *cmd, char *errbuf);

#ifdef __cpluplus
}
#endif