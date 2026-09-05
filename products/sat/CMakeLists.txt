bmo_add_plugin(BmoSaturator
    PRODUCT_NAME "BMO Saturator"
    PLUGIN_CODE  Bsat
    BUNDLE_ID    "com.lt3audio.bmosaturator"
    CATEGORIES   "Fx" "Distortion"
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    LIBRARIES    bmo_sat)
