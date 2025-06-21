idf_component_register(
    SRCS "test_core.c" "core.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES common network
)
