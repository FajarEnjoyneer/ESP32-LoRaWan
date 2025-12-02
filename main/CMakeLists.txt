idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_timer log freertos driver esp_driver_gpio)

# Opsi ini wajib ada untuk menekan error dari library RadioLib
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=overloaded-virtual")