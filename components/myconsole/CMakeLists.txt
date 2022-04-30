set(srce
    "console_main.c"    
    "cmd_nvs/cmd_nvs.c"
    "cmd_system/cmd_system.c"
    "cmd_wifi/cmd_wifi.c"
    "cmd_gsm/cmd_gsm.c"
    "cmd_gsm/console_helper2.c")

set(include_dirs
    "include"    
    "cmd_nvs/include"
    "cmd_system/include"
    "cmd_wifi/include"
    "cmd_gsm/include")

set(priv_include
    "cmd_gsm/priv_include")

set(req
    "console "
    "nvs_flash"
    "spi_flash")

set(priv_req
    "freertos" 
    "driver" 
    "console" 
    "fatfs"
    "sim800")

idf_component_register(SRCS ${srce}
                        INCLUDE_DIRS ${include_dirs}
                        PRIV_INCLUDE_DIRS ${priv_include}
                        REQUIRES ${req}
                        PRIV_REQUIRES ${priv_req})
