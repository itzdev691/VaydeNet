#pragma once

// The upstream bridge otherwise uses ESP-IDF logging directly.
#define ESPHOME_LOG_TAG(name, value) static const char* const name = value
