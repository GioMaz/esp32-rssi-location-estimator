idf_component_register(
    SRCS "state_training.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES utils gpio esp_wifi setup ap_scan
    REQUIRES dataset
)
