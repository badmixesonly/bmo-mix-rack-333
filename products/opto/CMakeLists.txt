bmo_add_plugin(BmoOpto
    PRODUCT_NAME "BMO Opto"
    PLUGIN_CODE  Bopt
    BUNDLE_ID    "com.lt3audio.bmoopto"
    CATEGORIES   "Fx" "Dynamics"
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    LIBRARIES    bmo_opto)
