set(source
    "lillygo.c"
    )

set(priv_req
    "freertos"
    "driver"
    )

idf_component_register(SRCS ${source} 
                        PRIV_REQUIRES ${priv_req}
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "priv_include")