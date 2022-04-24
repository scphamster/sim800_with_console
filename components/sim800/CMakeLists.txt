set(source
    "main/sim800.c" 
    "console/console_commands.c" 
    "console/console_helper.c")

idf_component_register(SRCS ${source} 
                        PRIV_REQUIRES "freertos" "driver" "lwip" "mbedtls" "log" "nvs_flash" "json" "gsm" "esp_common" "console" 
                        REQUIRES "esp_common" "freertos"
                        INCLUDE_DIRS "main/include" "console/include"
                        PRIV_INCLUDE_DIRS "console/private_include"
                        EMBED_TXTFILES server_root_cert.pem)
