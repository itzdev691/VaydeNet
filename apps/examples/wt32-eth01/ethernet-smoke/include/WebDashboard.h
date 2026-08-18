#pragma once

#include <cstdint>

#include <WebServer.h>

class EthernetNetwork;
class VaydeBroadcaster;

class WebDashboard final {
public:
    WebDashboard(
        EthernetNetwork &network,
        const VaydeBroadcaster &broadcaster);

    bool begin();
    void update();

private:
    void configureRoutes();
    void serveAsset(const char *path, const char *contentType);
    void sendStatus();
    void sendNotFound();

    EthernetNetwork &network_;
    const VaydeBroadcaster &broadcaster_;
    WebServer server_;
    bool filesystemReady_ = false;
    uint32_t announcedIp_ = 0;
    uint32_t lastAnnouncementCheckMs_ = 0;
};
