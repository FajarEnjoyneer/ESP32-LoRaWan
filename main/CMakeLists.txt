idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_timer log freertos)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=overloaded-virtual")