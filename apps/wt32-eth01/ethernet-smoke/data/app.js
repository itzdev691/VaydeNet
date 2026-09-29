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

function renderPortProbes(portProbes) {
  const container = elements["port-probes"];
  if (!container) return;

  setText(
    "port-probe-target",
    portProbes.targetAvailable ? portProbes.target : "Waiting for DHCP",
  );
  setText(
    "probe-sweeps",
    portProbes.completedSweeps === 0
      ? "No complete sweep yet"
      : `${integer.format(portProbes.completedSweeps)} complete ${
          portProbes.completedSweeps === 1 ? "sweep" : "sweeps"
        }`,
  );

  const cards = portProbes.ports.map((probe) => {
    const card = document.createElement("article");
    const state = !probe.tested ? "pending" : probe.open ? "open" : "closed";
    card.className = "probe-card";
    card.dataset.state = state;

    const label = document.createElement("span");
    label.textContent = `TCP ${probe.port}`;

    const value = document.createElement("strong");
    value.textContent =
      state === "pending"
        ? "Pending"
        : state === "open"
          ? "Open"
          : "Unavailable";

    const detail = document.createElement("small");
    detail.textContent = probe.tested
      ? `${integer.format(probe.latencyMs)} ms`
      : "Awaiting probe";

    card.append(label, value, detail);
    return card;
  });

  container.replaceChildren(...cards);
}

function render(status) {
  const { device, network, portProbes, espNow } = status;
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

  renderPortProbes(portProbes);

  setText("tx-accepted", integer.format(espNow.queueAccepted));
  setText("tx-attempts", `${integer.format(espNow.attempts)} attempts`);
  setText("tx-rejected", integer.format(espNow.queueRejected));
  setText(
    "last-result",
    `${espNow.lastQueueResult} (${espNow.lastQueueResultCode})`,
  );
  setText("destination", espNow.destination);
  setText("espnow-channel", `Channel ${espNow.channel}`);
  setText("station-mac", espNow.stationMac);
  setText("sequence", integer.format(espNow.sequenceNumber));
  setText("packet-size", `${integer.format(espNow.packetBytes)} bytes`);
  setText("delivery-success", integer.format(espNow.deliverySucceeded));
  setText("delivery-failed", integer.format(espNow.deliveryFailed));
  setText("last-delivery", espNow.lastDelivery);
  setText("accepted-bytes", formatBytes(espNow.queuedBytes));
  setText(
    "interval",
    espNow.intervalMs === 1000
      ? "1 second"
      : `${integer.format(espNow.intervalMs)} ms`,
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
