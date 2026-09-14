bmo_add_plugin(BmoVcomp
    PRODUCT_NAME "BMO Vcomp"
    PLUGIN_CODE  Bvcp
    BUNDLE_ID    "com.lt3audio.bmovcomp"
    CATEGORIES   "Fx" "Dynamics"
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    LIBRARIES    bmo_vcomp)
