bmo_add_plugin(BmoDimension
    PRODUCT_NAME "BMO Dimension"
    PLUGIN_CODE  Bdim
    BUNDLE_ID    "com.lt3audio.bmodimension"
    CATEGORIES   "Fx" "Tools"
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    LIBRARIES    bmo_dim)
