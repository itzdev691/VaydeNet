idf_component_register(
    SRCS "Nrf24Transport.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        VaydeEngine
        esp_driver_spi
    PRIV_REQUIRES
        esp_driver_gpio
        esp_rom
)
