idf_component_register(
    SRCS "EspNowTransport.cpp"
    INCLUDE_DIRS
        "."
        "../../VaydeEngine/include"
    PRIV_REQUIRES
        esp_event
        esp_netif
        esp_wifi
        nvs_flash
)
