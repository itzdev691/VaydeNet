set(esp_now_host_dependencies)
if(IDF_TARGET STREQUAL "esp32p4")
    list(APPEND esp_now_host_dependencies esp-now-hosted espressif__esp_hosted)
endif()

idf_component_register(
    SRCS "EspNowTransport.cpp"
    INCLUDE_DIRS
        "."
        "../../VaydeEngine/include"
    REQUIRES
        esp_wifi
        freertos
    PRIV_REQUIRES
        ${esp_now_host_dependencies}
        esp_event
        esp_netif
        nvs_flash
)
