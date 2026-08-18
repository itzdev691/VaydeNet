const elements = Object.fromEntries(
  [...document.querySelectorAll("[id]")].map((element) => [element.id, element]),
);

const integer = new Intl.NumberFormat();
let refreshInProgress = false;

function setText(id, value) {
  const element = elements[id];
  if (element) {
    element.textContent = value;
  }
}

function formatBytes(bytes) {
  if (bytes < 1024) {
    return `${integer.format(bytes)} B`;
  }

  const units = ["KiB", "MiB", "GiB"];
  let value = bytes / 1024;
  let unit = units[0];

  for (let index = 1; value >= 1024 && index < units.length; index += 1) {
    value /= 1024;
    unit = units[index];
  }

  return `${value.toFixed(value >= 10 ? 1 : 2)} ${unit}`;
}

function formatDuration(milliseconds) {
  const totalSeconds = Math.floor(milliseconds / 1000);
  const days = Math.floor(totalSeconds / 86400);
  const hours = Math.floor((totalSeconds % 86400) / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;

  const parts = [];
  if (days) parts.push(`${days}d`);
  if (hours || days) parts.push(`${hours}h`);
  if (minutes || hours || days) parts.push(`${minutes}m`);
  parts.push(`${seconds}s`);
  return parts.join(" ");
}

function render(status) {
  const { device, network, broadcast } = status;
  const linkOnline = network.linkUp && network.dhcpReady;

  elements.connection.dataset.state = linkOnline ? "online" : "offline";
  setText("connection-label", linkOnline ? "Board online" : "Network unavailable");
  setText("link-state", network.linkUp ? "Up" : "Down");
  setText(
    "link-mode",
    network.linkUp
      ? `${network.speedMbps} Mbps · ${network.duplex} duplex`
      : "No physical link",
  );

  setText("ipv4", network.dhcpReady ? network.ipv4 : "--");
  setText("hostname", device.hostname);
  setText(
    "dashboard-address",
    network.dhcpReady
      ? `http://${network.ipv4}:${device.dashboardPort}/`
      : "Waiting for DHCP",
  );
  setText("last-updated", `Updated ${new Date().toLocaleTimeString()}`);
  setText("dhcp-state", network.dhcpReady ? "DHCP ready" : "DHCP waiting");

  setText("mac", network.mac);
  setText("subnet", network.dhcpReady ? network.subnet : "--");
  setText("gateway", network.dhcpReady ? network.gateway : "--");
  setText("dns", network.dhcpReady ? network.dns : "--");
  setText("link-up-events", integer.format(network.linkUpEvents));
  setText("link-down-events", integer.format(network.linkDownEvents));

  setText("tx-accepted", integer.format(broadcast.driverAccepted));
  setText("tx-attempts", `${integer.format(broadcast.attempts)} attempts`);
  setText("tx-rejected", integer.format(broadcast.driverRejected));
  setText("last-result", `${broadcast.lastResult} (${broadcast.lastResultCode})`);
  setText("destination", broadcast.destination);
  setText("ether-type", broadcast.etherType);
  setText("sequence", integer.format(broadcast.sequenceNumber));
  setText("packet-size", `${integer.format(broadcast.packetBytes)} bytes`);
  setText("frame-size", `${integer.format(broadcast.frameBytes)} bytes`);
  setText("accepted-bytes", formatBytes(broadcast.acceptedBytes));
  setText(
    "interval",
    broadcast.intervalMs === 1000
      ? "1 second"
      : `${integer.format(broadcast.intervalMs)} ms`,
  );
  setText("uptime", `Uptime ${formatDuration(device.uptimeMs)}`);
}

function renderError() {
  elements.connection.dataset.state = "offline";
  setText("connection-label", "Status API unavailable");
  setText("last-updated", "Unable to refresh status");
}

async function refreshStatus() {
  if (refreshInProgress) return;
  refreshInProgress = true;

  try {
    const response = await fetch("/api/status", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`Status request failed: ${response.status}`);
    }

    render(await response.json());
  } catch (error) {
    console.error(error);
    renderError();
  } finally {
    refreshInProgress = false;
  }
}

refreshStatus();
window.setInterval(refreshStatus, 1000);
