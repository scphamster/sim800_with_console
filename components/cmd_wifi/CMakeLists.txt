
idf_component_register(SRCS "cmd_wifi.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "cmd_nvs" "cmd_system"
                       PRIV_REQUIRES "console")
