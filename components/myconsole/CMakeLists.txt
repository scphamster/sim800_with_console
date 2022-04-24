set(srce
    "console_main.c"    
    "cmd_nvs/cmd_nvs.c"
    "cmd_system/cmd_system.c"
    "cmd_wifi/cmd_wifi.c")

set(include_dirs
    "include"    
    "cmd_nvs/include"
    "cmd_system/include"
    "cmd_wifi/include")

set(req
    "console "
    "nvs_flash"
    "spi_flash")

set(priv_req
    "freertos" 
    "driver" 
    "console" 
    "fatfs")

idf_component_register(SRCS ${srce}
                        INCLUDE_DIRS ${include_dirs}
                        REQUIRES ${req}
                        PRIV_REQUIRES ${priv_req})
