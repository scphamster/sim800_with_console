#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Constants that aren't configurable in menuconfig
#define WEB_SERVER "loboris.eu"
#define WEB_PORT   80
#define WEB_URL    "http://loboris.eu/ESP32/info.txt"
#define WEB_URL1   "http://loboris.eu"

#define SSL_WEB_SERVER "www.howsmyssl.com"
#define SSL_WEB_URL    "https://www.howsmyssl.com/a/check"
#define SSL_WEB_PORT   "443"

#ifdef __cplusplus
}
#endif