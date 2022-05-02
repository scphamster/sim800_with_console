/* PPPoS Client Example with GSM (tested with Telit GL865-DUAL-V3)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include "sdkconfig.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "tcpip_adapter.h"
#include "netif/ppp/pppos.h"
#include "netif/ppp/ppp.h"
#include "netif/ppp/pppapi.h"

#include "gsm.h"

// === GSM configuration that you can set via 'make menuconfig'. ===
#define UART_GPIO_TX CONFIG_GSM_TX
#define UART_GPIO_RX CONFIG_GSM_RX
#define UART_BDRATE  CONFIG_GSM_BDRATE

#ifdef CONFIG_GSM_DEBUG
#define GSM_DEBUG                 1
#define GSM_LOG(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__);
#else
#define GSM_DEBUG                 0
#define GSM_LOG(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__);
#endif

#define BUF_SIZE                (1024)
#define GSM_OK_Str              "OK"
#define PPPOSMUTEX_TIMEOUT      1000 / portTICK_RATE_MS
#define PPPOS_CLIENT_STACK_SIZE 1024 * 3

#define GSM_END_TASK  ESP_FAIL
#define GSM_CONTINUE  ESP_OK
#define GSM_RECONNECT ESP_ERR_TIMEOUT

#define GSM_INIT_CMDS_NUM (sizeof(GSM_Init) / sizeof(GSM_Cmd *))

// shared variables, use mutex to access them
static uint8_t  g_gsm_status       = GSM_STATE_FIRSTINIT;
static int      g_do_pppos_connect = 1;
static uint32_t g_pppos_rx_count;
static uint32_t g_pppos_tx_count;
static uint8_t  g_pppos_task_started = 0;
static uint8_t  g_gsm_rfOff          = 0;

// local variables
static QueueHandle_t pppos_mutex       = NULL;
const char          *PPP_User          = CONFIG_GSM_INTERNET_USER;
const char          *PPP_Pass          = CONFIG_GSM_INTERNET_PASSWORD;
static int           g_uart_module_num = CONFIG_GSM_UART_MODULE_NUM;

static uint8_t  tcpip_adapter_initialized = 0;
static ppp_pcb *ppp                       = NULL;
struct netif    ppp_netif;

static const char *TAG = "[PPPOS CLIENT]";

typedef struct {
    char    *cmd;
    uint16_t cmdSize;
    char    *cmdResponseOnOk;
    uint16_t timeoutMs;
    uint16_t delayMs;
    uint8_t  skip;
} GSM_Cmd;

static GSM_Cmd cmd_AT = {
    .cmd             = "AT\r\n",
    .cmdSize         = sizeof("AT\r\n") - 1,
    .cmdResponseOnOk = GSM_OK_Str,
    .timeoutMs       = 300,
    .delayMs         = 0,
    .skip            = 0,
};

static GSM_Cmd cmd_NoSMSInd = {
    .cmd             = "AT+CNMI=0,0,0,0,0\r\n",
    .cmdSize         = sizeof("AT+CNMI=0,0,0,0,0\r\n") - 1,
    .cmdResponseOnOk = GSM_OK_Str,
    .timeoutMs       = 1000,
    .delayMs         = 0,
    .skip            = 0,
};

static GSM_Cmd cmd_Reset = {
    .cmd             = "ATZ\r\n",
    .cmdSize         = sizeof("ATZ\r\n") - 1,
    .cmdResponseOnOk = GSM_OK_Str,
    .timeoutMs       = 300,
    .delayMs         = 0,
    .skip            = 0,
};

static GSM_Cmd cmd_RFOn = {
    .cmd             = "AT+CFUN=1\r\n",
    .cmdSize         = sizeof("ATCFUN=1,0\r\n") - 1,
    .cmdResponseOnOk = GSM_OK_Str,
    .timeoutMs       = 10000,
    .delayMs         = 1000,
    .skip            = 0,
};

static GSM_Cmd cmd_EchoOff = {
    .cmd             = "ATE0\r\n",
    .cmdSize         = sizeof("ATE0\r\n") - 1,
    .cmdResponseOnOk = GSM_OK_Str,
    .timeoutMs       = 300,
    .delayMs         = 0,
    .skip            = 0,
};

static GSM_Cmd cmd_Pin = {
    .cmd             = "AT+CPIN?\r\n",
    .cmdSize         = sizeof("AT+CPIN?\r\n") - 1,
    .cmdResponseOnOk = "CPIN: READY",
    .timeoutMs       = 5000,
    .delayMs         = 0,
    .skip            = 0,
};

static GSM_Cmd cmd_Reg = {
    .cmd             = "AT+CREG?\r\n",
    .cmdSize         = sizeof("AT+CREG?\r\n") - 1,
    .cmdResponseOnOk = "CREG: 0,1",
    .timeoutMs       = 3000,
    .delayMs         = 2000,
    .skip            = 0,
};

static GSM_Cmd cmd_APN = {
    .cmd             = NULL,
    .cmdSize         = 0,
    .cmdResponseOnOk = GSM_OK_Str,
    .timeoutMs       = 8000,
    .delayMs         = 0,
    .skip            = 0,
};

static GSM_Cmd cmd_Connect = {
    .cmd     = "AT+CGDATA=\"PPP\",1\r\n",
    .cmdSize = sizeof("AT+CGDATA=\"PPP\",1\r\n") - 1,
    //.cmd = "ATDT*99***1#\r\n",
    //.cmdSize = sizeof("ATDT*99***1#\r\n")-1,
    .cmdResponseOnOk = "CONNECT",
    .timeoutMs       = 30000,
    .delayMs         = 1000,
    .skip            = 0,
};

static GSM_Cmd *GSM_Init[] = {
    &cmd_AT,  &cmd_Reset, &cmd_EchoOff, &cmd_RFOn,    &cmd_NoSMSInd,
    &cmd_Pin, &cmd_Reg,   &cmd_APN,     &cmd_Connect,
};

// PPP status callback
//--------------------------------------------------------------
static void
ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx)
{
    struct netif *pppif = ppp_netif(pcb);
    LWIP_UNUSED_ARG(ctx);

    switch (err_code) {
    case PPPERR_NONE: {
#if GSM_DEBUG
        ESP_LOGI(TAG, "status_cb: Connected");
#if PPP_IPV4_SUPPORT
        ESP_LOGI(TAG, "   ipaddr    = %s", ipaddr_ntoa(&pppif->ip_addr));
        ESP_LOGI(TAG, "   gateway   = %s", ipaddr_ntoa(&pppif->gw));
        ESP_LOGI(TAG, "   netmask   = %s", ipaddr_ntoa(&pppif->netmask));
#endif

#if PPP_IPV6_SUPPORT
        ESP_LOGI(TAG, "   ip6addr   = %s", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
#endif
#endif
        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        g_gsm_status = GSM_STATE_CONNECTED;
        xSemaphoreGive(pppos_mutex);
        break;
    }
    case PPPERR_PARAM: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Invalid parameter");
#endif
        break;
    }
    case PPPERR_OPEN: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Unable to open PPP session");
#endif
        break;
    }
    case PPPERR_DEVICE: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Invalid I/O device for PPP");
#endif
        break;
    }
    case PPPERR_ALLOC: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Unable to allocate resources");
#endif
        break;
    }
    case PPPERR_USER: {
/* ppp_free(); -- can be called here */
#if GSM_DEBUG
        ESP_LOGW(TAG, "status_cb: User interrupt (disconnected)");
#endif
        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        g_gsm_status = GSM_STATE_DISCONNECTED;
        xSemaphoreGive(pppos_mutex);
        break;
    }
    case PPPERR_CONNECT: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Connection lost");
#endif
        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        g_gsm_status = GSM_STATE_DISCONNECTED;
        xSemaphoreGive(pppos_mutex);
        break;
    }
    case PPPERR_AUTHFAIL: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Failed authentication challenge");
#endif
        break;
    }
    case PPPERR_PROTOCOL: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Failed to meet protocol");
#endif
        break;
    }
    case PPPERR_PEERDEAD: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Connection timeout");
#endif
        break;
    }
    case PPPERR_IDLETIMEOUT: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Idle Timeout");
#endif
        break;
    }
    case PPPERR_CONNECTTIME: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Max connect time reached");
#endif
        break;
    }
    case PPPERR_LOOPBACK: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Loopback detected");
#endif
        break;
    }
    default: {
#if GSM_DEBUG
        ESP_LOGE(TAG, "status_cb: Unknown error code %d", err_code);
#endif
        break;
    }
    }
}

// === Handle sending data to GSM modem ===
//------------------------------------------------------------------------------
static u32_t
ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx)
{
    uint32_t ret = uart_write_bytes(g_uart_module_num, (const char *)data, len);
    uart_wait_tx_done(g_uart_module_num, 10 / portTICK_RATE_MS);

    if (ret > 0) {
        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        g_pppos_rx_count += ret;
        xSemaphoreGive(pppos_mutex);
    }
    return ret;
}

//---------------------------------------------------------
static void
post_command_to_ESP_LOGI(char *cmd, int cmdSize, char *TAG2)
{
#if GSM_DEBUG

    char buf[cmdSize + 2];

    memset(buf, 0, cmdSize + 2);

    for (int i = 0; i < cmdSize; i++) {
        if ((cmd[i] != 0x00) && ((cmd[i] < 0x20) || (cmd[i] > 0x7F)))
            buf[i] = '.';
        else
            buf[i] = cmd[i];

        if (buf[i] == '\0')
            break;
    }

    ESP_LOGI(TAG, "%s [%s]", TAG2, buf);
#endif
}

//----------------------------------------------------------------------------------------------------------------------
static int
sendATcmd_checkAnswer(char  *cmd,
                      char  *desired_resp_pri,
                      char  *desired_resp_sec,
                      int    cmdSize,
                      int    timeout,
                      char **resp_return_buff,
                      int    size)
{
    char resp_buf[256] = { '\0' };
    char buf[256]      = { '\0' };
    int  uart_data_len;
    int  retval = 1, idx = 0, total_len = 0, timeoutCnt = 0;

    // ** Send command to GSM
    vTaskDelay(100 / portTICK_PERIOD_MS);
    uart_flush(g_uart_module_num);

    if (cmd != NULL) {
        if (cmdSize == -1)
            cmdSize = strlen(cmd);

#if GSM_DEBUG
        post_command_to_ESP_LOGI(cmd, cmdSize, "AT COMMAND:");
#endif

        uart_write_bytes(g_uart_module_num, (const char *)cmd, cmdSize);
        uart_wait_tx_done(g_uart_module_num, 100 / portTICK_RATE_MS);
    }

    if (resp_return_buff != NULL) {
        // Read GSM response into buffer
        char *pbuf = *resp_return_buff;

        uart_data_len =
          uart_read_bytes(g_uart_module_num, (uint8_t *)buf, 256, timeout / portTICK_RATE_MS);

        while (uart_data_len > 0) {
            if ((total_len + uart_data_len) >= size) {
                char *ptemp = realloc(pbuf, size + 512);

                if (ptemp == NULL)
                    return 0;

                size += 512;
                pbuf = ptemp;
            }

            memcpy(pbuf + total_len, buf, uart_data_len);
            total_len += uart_data_len;
            resp_return_buff[total_len] = '\0';

            uart_data_len =
              uart_read_bytes(g_uart_module_num, (uint8_t *)buf, 256, 100 / portTICK_RATE_MS);
        }

        *resp_return_buff = pbuf;
        return total_len;
    }

    // ** Wait for and check the response
    idx = 0;
    while (1) {
        memset(buf, 0, 256);
        uart_data_len = 0;

        uart_data_len =
          uart_read_bytes(g_uart_module_num, (uint8_t *)buf, 256, 10 / portTICK_RATE_MS);

        if (uart_data_len > 0) {
            for (int i = 0; i < uart_data_len; i++) {
                if (idx < 256) {
                    if ((buf[i] >= 0x20) && (buf[i] < 0x80))
                        resp_buf[idx++] = buf[i];
                    else
                        resp_buf[idx++] = 0x2e;
                }
            }
            total_len += uart_data_len;
        }
        else if (total_len > 0) {
            // Check the response
            if (strstr(resp_buf, desired_resp_pri) != NULL) {
#if GSM_DEBUG
                ESP_LOGI(TAG, "AT RESPONSE: [%s]", resp_buf);
#endif

                break;
            }
            else if ((desired_resp_sec != NULL) && (strstr(resp_buf, desired_resp_sec) != NULL)) {
#if GSM_DEBUG
                ESP_LOGI(TAG, "AT RESPONSE (1): [%s]", resp_buf);
#endif

                retval = 2;
                break;
            }
            else {
// no match
#if GSM_DEBUG
                ESP_LOGI(TAG, "AT BAD RESPONSE: [%s]", resp_buf);
#endif

                retval = 0;
                break;
            }
        }

        timeoutCnt += 10;

        if (timeoutCnt > timeout) {
// timeout
#if GSM_DEBUG
            ESP_LOGE(TAG, "AT: TIMEOUT");
#endif

            retval = 0;
            break;
        }
    }

    return retval;
}

//------------------------------------
static void
_disconnect(uint8_t rfOff)
{
    int res = sendATcmd_checkAnswer("AT\r\n", GSM_OK_Str, NULL, 4, 1000, NULL, 0);

    if (res == 1) {
        if (rfOff) {
            cmd_Reg.timeoutMs = 10000;
            res               = sendATcmd_checkAnswer("AT+CFUN=4\r\n",
                                        GSM_OK_Str,
                                        NULL,
                                        11,
                                        10000,
                                        NULL,
                                        0);   // disable RF function
        }
        return;
    }

    GSM_LOG(TAG, "ONLINE, DISCONNECTING...");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    uart_flush(g_uart_module_num);
    uart_write_bytes(g_uart_module_num, "+++", 3);
    uart_wait_tx_done(g_uart_module_num, 10 / portTICK_RATE_MS);
    vTaskDelay(1100 / portTICK_PERIOD_MS);

    int n = 0;
    res   = sendATcmd_checkAnswer("ATH\r\n", GSM_OK_Str, "NO CARRIER", 5, 3000, NULL, 0);

    while (res == 0) {
        n++;

        if (n > 10) {
            GSM_LOG(TAG, "STILL CONNECTED.");

            n = 0;

            vTaskDelay(1000 / portTICK_PERIOD_MS);
            uart_flush(g_uart_module_num);
            uart_write_bytes(g_uart_module_num, "+++", 3);
            uart_wait_tx_done(g_uart_module_num, 10 / portTICK_RATE_MS);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
        res = sendATcmd_checkAnswer("ATH\r\n", GSM_OK_Str, "NO CARRIER", 5, 3000, NULL, 0);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    if (rfOff) {
        cmd_Reg.timeoutMs = 10000;
        res = sendATcmd_checkAnswer("AT+CFUN=4\r\n", GSM_OK_Str, NULL, 11, 3000, NULL, 0);
    }

    GSM_LOG(TAG, "DISCONNECTED.");
}

//----------------------------
static void
_reset_init_commands()
{
    for (int idx = 0; idx < GSM_INIT_CMDS_NUM; idx++) {
        GSM_Init[idx]->skip = 0;
    }
}

static inline esp_err_t
_configure_uart(void)
{
    if (gpio_set_direction(UART_GPIO_TX, GPIO_MODE_OUTPUT))
        return ESP_FAIL;
    if (gpio_set_direction(UART_GPIO_RX, GPIO_MODE_INPUT))
        return ESP_FAIL;
    if (gpio_set_pull_mode(UART_GPIO_RX, GPIO_PULLUP_ONLY))
        return ESP_FAIL;

    uart_config_t uart_config = { .baud_rate = UART_BDRATE,
                                  .data_bits = UART_DATA_8_BITS,
                                  .parity    = UART_PARITY_DISABLE,
                                  .stop_bits = UART_STOP_BITS_1,
                                  .flow_ctrl = UART_HW_FLOWCTRL_DISABLE };

    if (uart_param_config(g_uart_module_num, &uart_config))
        return ESP_FAIL;

    if (uart_set_pin(g_uart_module_num,
                     UART_GPIO_TX,
                     UART_GPIO_RX,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE))
        return ESP_FAIL;
    if (uart_driver_install(g_uart_module_num, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0))
        return ESP_FAIL;

    return ESP_OK;
}

static inline void
_configure_command_APN(void)
{
    // Set APN from config
    static char PPP_ApnATReq[sizeof(CONFIG_GSM_APN) + 24];

    sprintf(PPP_ApnATReq, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", CONFIG_GSM_APN);
    cmd_APN.cmd     = PPP_ApnATReq;
    cmd_APN.cmdSize = strlen(PPP_ApnATReq);
}

static esp_err_t
_send_gsm_config_commands(void)
{
    int init_cmd_num = 0;
    int n_fail       = 0;

    GSM_LOG(TAG, "GSM initialization start");

    vTaskDelay(500 / portTICK_PERIOD_MS);

    _reset_init_commands();

    // * GSM Initialization loop
    while (init_cmd_num < GSM_INIT_CMDS_NUM) {
        if (GSM_Init[init_cmd_num]->skip) {
            post_command_to_ESP_LOGI(GSM_Init[init_cmd_num]->cmd,
                                     GSM_Init[init_cmd_num]->cmdSize,
                                     "Skip command:");

            init_cmd_num++;
            continue;
        }

        if (sendATcmd_checkAnswer(GSM_Init[init_cmd_num]->cmd,
                                  GSM_Init[init_cmd_num]->cmdResponseOnOk,
                                  NULL,
                                  GSM_Init[init_cmd_num]->cmdSize,
                                  GSM_Init[init_cmd_num]->timeoutMs,
                                  NULL,
                                  0) == 0) {
            // * No response or not as expected, start from first initialization command
            GSM_LOG(TAG, "Wrong response, restarting...");

            n_fail++;
            if (n_fail > 20)
                return ESP_FAIL;

            vTaskDelay(3000 / portTICK_PERIOD_MS);
            init_cmd_num = 0;

            continue;
        }

        if (GSM_Init[init_cmd_num]->delayMs > 0)
            vTaskDelay(GSM_Init[init_cmd_num]->delayMs / portTICK_PERIOD_MS);

        GSM_Init[init_cmd_num]->skip = 1;

        if (GSM_Init[init_cmd_num] == &cmd_Reg)
            GSM_Init[init_cmd_num]->delayMs = 0;

        // Next command
        init_cmd_num++;
    }

    return ESP_OK;
}

static esp_err_t
_pppapi_pppos_create(void)
{
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    g_pppos_tx_count = 0;
    g_pppos_rx_count = 0;
    g_gsm_status     = GSM_STATE_FIRSTINIT;
    xSemaphoreGive(pppos_mutex);

    // initialize pppos
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    if (g_gsm_status == GSM_STATE_FIRSTINIT) {
        xSemaphoreGive(pppos_mutex);

        // ** After first successful initialization create PPP control block
        GSM_LOG(TAG, "pppapi_pppos_create\n file = %s, line = %d", __FILE__, __LINE__);
        ppp = pppapi_pppos_create(&ppp_netif, ppp_output_callback, ppp_status_cb, NULL);

        if (ppp == NULL) {
            return ESP_FAIL;
        }
    }
    else
        xSemaphoreGive(pppos_mutex);

    pppapi_set_default(ppp);
    pppapi_set_auth(ppp, PPPAUTHTYPE_PAP, PPP_User, PPP_Pass);
    // pppapi_set_auth(ppp, PPPAUTHTYPE_NONE, PPP_User, PPP_Pass);

    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    g_gsm_status = GSM_STATE_IDLE;
    xSemaphoreGive(pppos_mutex);

    pppapi_connect(ppp, 0);

    return ESP_OK;
}

static void
_handle_data_from_gsm(char *databuf)
{
    int len;

    memset(databuf, 0, BUF_SIZE);

    len = uart_read_bytes(g_uart_module_num, (uint8_t *)databuf, BUF_SIZE, 30 / portTICK_RATE_MS);

    if (len > 0) {
        pppos_input_tcpip(ppp, (u8_t *)databuf, len);

        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        g_pppos_tx_count += len;
        xSemaphoreGive(pppos_mutex);
    }
}

static esp_err_t
_check_and_handle_disconnect_request(char *databuf)
{
    int end_task;
    int gsm_status;

    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);

    if (g_do_pppos_connect > 0) {
        xSemaphoreGive(pppos_mutex);

        return GSM_CONTINUE;
    }

    end_task           = g_do_pppos_connect;
    g_do_pppos_connect = 1;
    xSemaphoreGive(pppos_mutex);

    GSM_LOG(TAG, "Disconnect requested.");

    pppapi_close(ppp, 0);
    gsm_status = GSM_STATE_CONNECTED;

    // wait untill disconnected
    while (gsm_status != GSM_STATE_DISCONNECTED) {
        _handle_data_from_gsm(databuf);

        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        gsm_status = g_gsm_status;
        xSemaphoreGive(pppos_mutex);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    uint8_t rfoff = g_gsm_rfOff;
    xSemaphoreGive(pppos_mutex);

    // Disconnect GSM if still connected
    _disconnect(rfoff);
    GSM_LOG(TAG, "Disconnected.");

    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    g_gsm_status       = GSM_STATE_IDLE;
    g_do_pppos_connect = 0;
    xSemaphoreGive(pppos_mutex);

    if (end_task < 0)
        return GSM_END_TASK;

    // === Wait for reconnect request ===
    gsm_status = GSM_STATE_DISCONNECTED;

    while (gsm_status == GSM_STATE_DISCONNECTED) {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        gsm_status = g_do_pppos_connect;
        xSemaphoreGive(pppos_mutex);
    }

    GSM_LOG(TAG, "Reconnect requested.");

    return GSM_RECONNECT;
}

static inline bool
_gsm_is_disconnected(void)
{
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    // === Check if disconnected ===
    if (g_gsm_status == GSM_STATE_DISCONNECTED) {
        xSemaphoreGive(pppos_mutex);

        GSM_LOG(TAG, "Disconnected, trying again...");
        pppapi_close(ppp, 0);
        g_gsm_status = GSM_STATE_IDLE;

        vTaskDelay(10000 / portTICK_PERIOD_MS);

        return true;
    }
    else {
        xSemaphoreGive(pppos_mutex);

        return false;
    }
}

static void
pppos_client_task()
{
    char     *data;
    esp_err_t retval;

    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    g_pppos_task_started = 1;
    xSemaphoreGive(pppos_mutex);

    // Allocate receive buffer
    data = (char *)malloc(BUF_SIZE);
    if (data == NULL) {
        GSM_LOG(TAG, "Failed to allocate data buffer.");
        goto exit;
    }

    retval = _configure_uart();
    if (retval != ESP_OK) {
        GSM_LOG(TAG, "Failed with UART configuration");
        goto exit;
    }

    _configure_command_APN();
    _disconnect(1);

    retval = _pppapi_pppos_create();
    if (retval != ESP_OK) {
        GSM_LOG(TAG, "Error initializing PPPoS");
        goto exit;
    }

    while (1) {
        retval = _send_gsm_config_commands();
        if (retval != ESP_OK) {
            GSM_LOG(TAG, "GSM initiallization failed");
            goto exit;
        }

        GSM_LOG(TAG, "GSM initialized.\ng_gsm_status = %d", g_gsm_status);

        // *** LOOP: Handle GSM modem responses & disconnects ***
        while (1) {
            retval = _check_and_handle_disconnect_request(data);
            if (retval == GSM_END_TASK) {
                goto exit;
            }
            else if (retval == GSM_RECONNECT) {
                break;
            }

            if (_gsm_is_disconnected()) {
                break;
            }
            
            _handle_data_from_gsm(data);

        }   // Handle GSM modem responses & disconnects loop
    }       // main task loop

exit:
    if (data)
        free(data);
    if (ppp)
        ppp_free(ppp);

    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    g_pppos_task_started = 0;
    g_gsm_status         = GSM_STATE_FIRSTINIT;
    xSemaphoreGive(pppos_mutex);

    GSM_LOG(TAG, "PPPoS TASK TERMINATED");
    vTaskDelete(NULL);
}

//=============
int
ppposInit()
{
    if (pppos_mutex != NULL)
        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);

    g_do_pppos_connect = 1;

    int gsm_status         = 0;
    int pppos_task_started = g_pppos_task_started;

    if (pppos_mutex != NULL)
        xSemaphoreGive(pppos_mutex);

    if (pppos_task_started == 0) {
        if (pppos_mutex == NULL)
            pppos_mutex = xSemaphoreCreateMutex();

        if (pppos_mutex == NULL)
            return 0;

        if (tcpip_adapter_initialized == 0) {
            tcpip_adapter_init();
            tcpip_adapter_initialized = 1;
        }

        xTaskCreate(pppos_client_task,
                    "pppos_client_task",
                    PPPOS_CLIENT_STACK_SIZE,
                    NULL,
                    10,
                    NULL);

        while (pppos_task_started == 0) {
            vTaskDelay(10 / portTICK_RATE_MS);

            xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
            pppos_task_started = g_pppos_task_started;
            xSemaphoreGive(pppos_mutex);
        }
    }

    while (gsm_status != 1) {
        vTaskDelay(10 / portTICK_RATE_MS);

        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        gsm_status         = g_gsm_status;
        pppos_task_started = g_pppos_task_started;
        xSemaphoreGive(pppos_mutex);

        if (pppos_task_started == 0)
            return 0;
    }

    return 1;
}

//===================================================
void
ppposDisconnect(uint8_t end_task, uint8_t rfoff)
{
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    int gsm_status = g_gsm_status;
    xSemaphoreGive(pppos_mutex);

    if (gsm_status == GSM_STATE_IDLE)
        return;

    gsm_status = 0;

    vTaskDelay(2000 / portTICK_RATE_MS);

    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    if (end_task)
        g_do_pppos_connect = -1;
    else
        g_do_pppos_connect = 0;

    g_gsm_rfOff = rfoff;

    xSemaphoreGive(pppos_mutex);

    while (gsm_status == 0) {
        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        gsm_status = g_do_pppos_connect;
        xSemaphoreGive(pppos_mutex);

        vTaskDelay(10 / portTICK_RATE_MS);
    }
    while (gsm_status != 0) {
        vTaskDelay(100 / portTICK_RATE_MS);

        xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
        gsm_status = g_do_pppos_connect;
        xSemaphoreGive(pppos_mutex);
    }
}

//===================
int
ppposStatus()
{
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    int gstat = g_gsm_status;
    xSemaphoreGive(pppos_mutex);

    return gstat;
}

//========================================================
void
getRxTxCount(uint32_t *rx, uint32_t *tx, uint8_t rst)
{
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    *rx = g_pppos_rx_count;
    *tx = g_pppos_tx_count;
    if (rst) {
        g_pppos_rx_count = 0;
        g_pppos_tx_count = 0;
    }
    xSemaphoreGive(pppos_mutex);
}

//===================
void
resetRxTxCount()
{
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    g_pppos_rx_count = 0;
    g_pppos_tx_count = 0;
    xSemaphoreGive(pppos_mutex);
}

//=============
int
gsm_RFOff()
{
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    int gstat = g_gsm_status;
    xSemaphoreGive(pppos_mutex);

    if (gstat != GSM_STATE_IDLE)
        return 0;

    uint8_t f       = 1;
    char    buf[64] = { '\0' };
    char   *pbuf    = buf;
    int     res     = sendATcmd_checkAnswer("AT+CFUN?\r\n", NULL, NULL, -1, 2000, &pbuf, 63);

    if (res > 0) {
        if (strstr(buf, "+CFUN: 4"))
            f = 0;
    }

    if (f) {
        cmd_Reg.timeoutMs = 500;
        return sendATcmd_checkAnswer("AT+CFUN=4\r\n",
                                     GSM_OK_Str,
                                     NULL,
                                     11,
                                     10000,
                                     NULL,
                                     0);   // disable RF function
    }
    return 1;
}

//============
int
gsm_RFOn()
{
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    int gstat = g_gsm_status;
    xSemaphoreGive(pppos_mutex);

    if (gstat != GSM_STATE_IDLE)
        return 0;

    uint8_t f       = 1;
    char    buf[64] = { '\0' };
    char   *pbuf    = buf;

    int res = sendATcmd_checkAnswer("AT+CFUN?\r\n", NULL, NULL, -1, 2000, &pbuf, 63);

    if (res > 0) {
        if (strstr(buf, "+CFUN: 1"))
            f = 0;
    }

    if (f) {
        cmd_Reg.timeoutMs = 0;
        return sendATcmd_checkAnswer("AT+CFUN=1\r\n",
                                     GSM_OK_Str,
                                     NULL,
                                     11,
                                     10000,
                                     NULL,
                                     0);   // disable RF function
    }

    return 1;
}

//--------------------
static int
module_is_in_text_mode_and_active()
{
    if (ppposStatus() != GSM_STATE_IDLE)
        return 0;

    int res = sendATcmd_checkAnswer("AT+CFUN?\r\n", "+CFUN: 1", NULL, -1, 1000, NULL, 0);

    if (res != 1)
        return 0;

    res = sendATcmd_checkAnswer("AT+CMGF=1\r\n", GSM_OK_Str, NULL, -1, 1000, NULL, 0);

    if (res != 1)
        return 0;

    return 1;
}

//==================================
int
smsSend(char *smsnum, char *msg)
{
    if (module_is_in_text_mode_and_active() == 0)
        return 0;

    char buf[64];
    int  len;
    int  res;

    len = strlen(msg);
    sprintf(buf, "AT+CMGS=\"%s\"\r\n", smsnum);
    res = sendATcmd_checkAnswer(buf, "> ", NULL, -1, 1000, NULL, 0);

    if (res != 1) {
        res = sendATcmd_checkAnswer("\x1B", GSM_OK_Str, NULL, 1, 1000, NULL, 0);
        return 0;
    }

    char *msgbuf = malloc(len + 2);
    if (msgbuf == NULL)
        return 0;

    sprintf(msgbuf, "%s\x1A", msg);
    res = sendATcmd_checkAnswer(msgbuf, "+CMGS: ", "ERROR", len + 1, 40000, NULL, 0);
    if (res != 1) {
        res = sendATcmd_checkAnswer("\x1B", GSM_OK_Str, NULL, 1, 1000, NULL, 0);
        res = 0;
    }

    free(msgbuf);
    return res;
}

/**
 * @brief Get number of messages in buffer
 *
 * @param rbuffer buffer for messages
 * @return int number of messages
 */
static int
numSMS(char *rbuffer)
{
    if (strlen(rbuffer) == 0)
        return 0;

    char *msgidx = rbuffer;
    int   nmsg   = 0;
    while (1) {
        msgidx = strstr(msgidx, "+CMGL: ");
        if (msgidx == NULL)
            break;
        nmsg++;
        msgidx += 7;
    }
    return nmsg;
}

/**
 * @brief Parse message at index idx to message structure
 *
 * @param rbuffer buffer for messages
 * @param idx index of message
 * @param msg messages structure
 * @return int
 */
static int
getSMS(char *rbuffer, int idx, GSM_sms_msg *msg)
{
    // Find requested message pointer
    char   *msgidx        = rbuffer;
    int     nmsg          = 0;
    uint8_t cmgl_text_len = 7;

    while (1) {
        msgidx = strstr(msgidx, "+CMGL: ");

        if (msgidx == NULL)
            break;

        nmsg++;
        msgidx += cmgl_text_len;

        if (nmsg == idx)
            break;
    }

    if (nmsg != idx)
        return 0;

    // Clear message structure
    memset(msg, 0, sizeof(GSM_sms_msg));

    // Get message info
    char *pend = strstr(msgidx, "\r\n");
    if (pend == NULL)
        return 0;

    int  len = pend - msgidx;
    char hdr[len + 4];
    char buf[32];

    memset(hdr, 0, len + 4);
    memcpy(hdr, msgidx, len);
    hdr[len] = '\0';

    // Get message body
    msgidx = pend + 2;
    pend   = strstr(msgidx, "\r\n");
    if (pend == NULL)
        return 0;

    // Allocate message body buffer and copy the data
    len           = pend - msgidx;
    char *msgdata = calloc(len + 2, 1);
    memcpy(msgdata, msgidx, len);
    msg->msg = msgdata;

    // Parse message info
    msgidx = hdr;
    pend   = strstr(hdr, ",\"");
    int i  = 1;

    while (pend != NULL) {
        len = pend - msgidx;

        if ((len < 32) && (len > 0)) {
            memset(buf, 0, 32);
            strncpy(buf, msgidx, len);
            buf[len] = '\0';
            if (buf[len - 1] == '"')
                buf[len - 1] = '\0';

            if (i == 1)
                msg->idx = (int)strtol(buf, NULL, 0);   // message index
            else if (i == 2)
                strcpy(msg->stat, buf);   // message status
            else if (i == 3)
                strcpy(msg->from, buf);   // phone number of message sender
            else if (i == 5)
                strcpy(msg->time, buf);   // the time when the message was sent
        }

        i++;
        msgidx = pend + 2;
        pend   = strstr(msgidx, ",\"");

        if (pend == NULL)
            pend = strstr(msgidx, "\"");
    }

    if (strlen(msg->time) >= 20) {
        // Convert message time to time structure
        int       hh, mm, ss, yy, mn, dd, tz;
        struct tm tm;

        sscanf(msg->time, "%u/%u/%u,%u:%u:%u%d", &yy, &mn, &dd, &hh, &mm, &ss, &tz);
        tm.tm_hour      = hh;
        tm.tm_min       = mm;
        tm.tm_sec       = ss;
        tm.tm_year      = yy + 100;
        tm.tm_mon       = mn;
        tm.tm_mday      = dd;
        msg->time_value = mktime(&tm);   // Linux time
        msg->tz         = tz / 4;        // time zone info
    }

    return nmsg;
}

void
smsRead(GSM_sms *SMSmesg, int sort)
{
    SMSmesg->messages = NULL;
    SMSmesg->nmsg     = 0;

    if (module_is_in_text_mode_and_active() == 0)
        return;

    int   size    = 512;
    char *rbuffer = malloc(size);
    if (rbuffer == NULL)
        return;

    int res = sendATcmd_checkAnswer("AT+CMGL=\"ALL\"\r\n", NULL, NULL, -1, 2000, &rbuffer, size);
    if (res <= 0) {
        free(rbuffer);

        return;
    }

    int nmsg = numSMS(rbuffer);
    if (nmsg > 0) {
        // Allocate buffer for nmsg messages
        GSM_sms_msg *messages = calloc(nmsg, sizeof(GSM_sms_msg));
        if (messages == NULL) {
            free(rbuffer);
            return;
        }

        GSM_sms_msg msg;

        for (int i = 0; i < nmsg; i++) {
            if (getSMS(rbuffer, i + 1, &msg) > 0) {
                memcpy(messages + (i * sizeof(GSM_sms_msg)), &msg, sizeof(GSM_sms_msg));
                SMSmesg->nmsg++;
            }
        }

        if ((SMSmesg->nmsg) && (sort != 0)) {
            GSM_sms_msg *smessages = calloc(SMSmesg->nmsg, sizeof(GSM_sms_msg));
            uint8_t      not_processed[SMSmesg->nmsg];
            memset(not_processed, 1, SMSmesg->nmsg);

            if (sort > 0) {
                for (int idx = 0; idx < SMSmesg->nmsg; idx++) {
                    // find minimal time
                    time_t tm = 0x7FFFFFFF;

                    for (int i = 0; i < SMSmesg->nmsg; i++) {
                        if (not_processed[i]) {
                            if ((messages + (i * sizeof(GSM_sms_msg)))->time_value < tm)
                                tm = (messages + (i * sizeof(GSM_sms_msg)))->time_value;
                        }
                    }
                    // Copy the message
                    for (int i = 0; i < SMSmesg->nmsg; i++) {
                        if (not_processed[i]) {
                            if ((messages + (i * sizeof(GSM_sms_msg)))->time_value == tm) {
                                memcpy(smessages + (idx * sizeof(GSM_sms_msg)),
                                       messages + (i * sizeof(GSM_sms_msg)),
                                       sizeof(GSM_sms_msg));
                                not_processed[i] = 0;   // mark as processed
                                break;
                            }
                        }
                    }
                }
            }
            else {
                for (int idx = 0; idx < SMSmesg->nmsg; idx++) {
                    // find maximal time
                    time_t tm = 0;
                    for (int i = 0; i < SMSmesg->nmsg; i++) {
                        if (not_processed[i]) {
                            if ((messages + (i * sizeof(GSM_sms_msg)))->time_value > tm)
                                tm = (messages + (i * sizeof(GSM_sms_msg)))->time_value;
                        }
                    }
                    // Copy the message
                    for (int i = 0; i < SMSmesg->nmsg; i++) {
                        if (not_processed[i]) {
                            if ((messages + (i * sizeof(GSM_sms_msg)))->time_value == tm) {
                                memcpy(smessages + (idx * sizeof(GSM_sms_msg)),
                                       messages + (i * sizeof(GSM_sms_msg)),
                                       sizeof(GSM_sms_msg));
                                not_processed[i] = 0;   // mark as processed
                                break;
                            }
                        }
                    }
                }
            }
            SMSmesg->messages = smessages;
            free(messages);
        }
        else {
            if (SMSmesg->nmsg)
                SMSmesg->messages = messages;
            else
                free(messages);
        }
    }

    free(rbuffer);
}

int
smsDelete(int idx)
{
    if (module_is_in_text_mode_and_active() == 0)
        return 0;

    char buf[64];

    sprintf(buf, "AT+CMGD=%d\r\n", idx);

    return sendATcmd_checkAnswer(buf, GSM_OK_Str, NULL, -1, 5000, NULL, 0);
}
