#pragma once

#include "esp_err.h"
#include "gsm.h"

#ifdef __cplusplus
extern "C" {
#endif

void      sms_prepare_defaults(cmd_gsm_queue_item_t *config);
void      sms_prepare_to_use_sms(void);
esp_err_t sms_check_new_config(char *number, char *text);
esp_err_t sms_copy_config_data(cmd_gsm_queue_item_t *config, cmd_gsm_queue_item_t *newconfig);
esp_err_t sms_send_sms(const char *sms_number, const char *sms_text);
esp_err_t sms_handle_received_sms(GSM_sms *messages, cmd_gsm_queue_item_t *config);

#ifdef __cplusplus
}
#endif