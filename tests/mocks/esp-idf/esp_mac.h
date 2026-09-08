#pragma once

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(address) \
    (address)[0], \
    (address)[1], \
    (address)[2], \
    (address)[3], \
    (address)[4], \
    (address)[5]
