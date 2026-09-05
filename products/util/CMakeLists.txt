bmo_add_plugin(BmoUtil
    PRODUCT_NAME "BMO Util"
    PLUGIN_CODE  Butl
    BUNDLE_ID    "com.lt3audio.bmoutil"
    CATEGORIES   "Fx" "Tools"
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    LIBRARIES    bmo_util)
