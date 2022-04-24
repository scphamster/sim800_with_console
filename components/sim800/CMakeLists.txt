set(source
    "main/sim800.c" 
    "console/console.c" 
    "console/console_helper.c"
    "gsm/gsm.c")

set(priv_req
    "freertos" 
    "driver" 
    "lwip" 
    "mbedtls" 
    "log" 
    "nvs_flash" 
    "json" 
    "esp_common" 
    "console" )

idf_component_register(SRCS ${source} 
                        PRIV_REQUIRES ${priv_req}
                        REQUIRES "esp_common" "freertos"
                        INCLUDE_DIRS "main/include" "console/include" "gsm/include"
                        PRIV_INCLUDE_DIRS "console/private_include"
                        EMBED_TXTFILES server_root_cert.pem)
