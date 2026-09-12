#pragma once

void testEspLog(const char* tag, const char* format, ...);

#define ESP_LOGI(tag, format, ...) \
    testEspLog(tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) \
    testEspLog(tag, format, ##__VA_ARGS__)
