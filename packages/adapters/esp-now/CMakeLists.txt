idf_component_register(
    SRCS "EspNowTransport.cpp"
    INCLUDE_DIRS
        "."
        "../../VaydeEngine/include"
    REQUIRES
        esp_wifi
        freertos
    PRIV_REQUIRES
        esp_event
        esp_netif
        nvs_flash
)
