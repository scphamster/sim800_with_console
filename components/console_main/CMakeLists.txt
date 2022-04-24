idf_component_register(SRCS "console_main.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES "freertos" "driver" "console" "cmd_wifi" "fatfs")
