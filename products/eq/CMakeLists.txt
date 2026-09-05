bmo_add_plugin(BmoEq
    PRODUCT_NAME "BMO EQ"
    PLUGIN_CODE  Fsty
    BUNDLE_ID    "com.lt3audio.frostyeq"
    CATEGORIES   "Fx" "EQ"
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    LIBRARIES    bmo_eq)
