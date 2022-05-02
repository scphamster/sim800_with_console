set(source
    "sim800.c" 
    #"console/console.c" 
    #"console/console_helper.c"
    "gsm/gsm.c"
    "sms/sms.c")

set(priv_req
    "freertos" 
    "driver" 
    "lwip" 
    "mbedtls" 
    "log" 
    "nvs_flash" 
    "json" 
    "esp_common" 
    "console"
    "myconsole"
    "lillygo" )

set(priv_include
    #"console/private_include"
    "priv_include")

set(include
    "include" 
    #"console/include" 
    "gsm/include"
    "sms/include")

idf_component_register(SRCS ${source} 
                        PRIV_REQUIRES ${priv_req}
                        REQUIRES "esp_common" "freertos"
                        INCLUDE_DIRS ${include}
                        PRIV_INCLUDE_DIRS ${priv_include}
                        EMBED_TXTFILES server_root_cert.pem)

add_compile_options(-Wfatal-errors)