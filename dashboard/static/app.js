/**
 * M.I.N.D. Companion Dashboard — Client-Side JS
 * Connects to Flask via Server-Sent Events (SSE).
 * All real-time panel updates happen here.
 */

// ── State ──────────────────────────────────────────────────────
const MAX_HR_HISTORY = 60;

let hrHistory    = [];
let connected    = false;
let camAutoOpened = false;  // true when camera was opened by firmware (not user)

// ── DOM refs ───────────────────────────────────────────────────
const $ = id => document.getElementById(id);

const connBadge   = $("conn-badge");
const connLabel   = $("conn-label");
const waitConn    = $("wait-conn");
const waitData    = $("wait-data");
const sensorGrid  = $("sensor-grid");
const emergency   = $("emergency-banner");

// ── HR chart (plain Canvas — full width section) ──────────────
const hrCanvas = $("hr-chart");
const hrCtx    = hrCanvas.getContext("2d");
const hrGraphSection = $("hr-graph-section");

function drawHrChart() {
  const w = hrCanvas.offsetWidth || 600;
  const h = 140;
  hrCanvas.width  = w * (window.devicePixelRatio || 1);
  hrCanvas.height = h * (window.devicePixelRatio || 1);
  hrCtx.scale(window.devicePixelRatio || 1, window.devicePixelRatio || 1);

  hrCtx.clearRect(0, 0, w, h);
  if (hrHistory.length < 2) return;

  const vals  = hrHistory.filter(v => v > 0);
  if (vals.length === 0) return;

  const min = Math.max(0, Math.min(...vals) - 10);
  const max = Math.max(...vals) + 10;
  const range = max - min || 1;

  const isAbnormal = vals.some(v => v > 120 || v < 50);
  const color = isAbnormal ? "#f87171" : "#4ade80";

  const padY = 12;
  const pts = hrHistory.map((v, i) => ({
    x: (i / (hrHistory.length - 1)) * w,
    y: v > 0 ? (h - padY) - ((v - min) / range) * (h - padY * 2) : null,
  }));

  // Fill gradient
  const grad = hrCtx.createLinearGradient(0, 0, 0, h);
  grad.addColorStop(0, color + "33");
  grad.addColorStop(1, color + "03");

  hrCtx.beginPath();
  let first = true;
  for (const p of pts) {
    if (p.y === null) { first = true; continue; }
    first ? hrCtx.moveTo(p.x, p.y) : hrCtx.lineTo(p.x, p.y);
    first = false;
  }
  const lastValid  = [...pts].reverse().find(p => p.y !== null);
  const firstValid = pts.find(p => p.y !== null);
  if (lastValid && firstValid) {
    hrCtx.lineTo(lastValid.x, h);
    hrCtx.lineTo(firstValid.x, h);
    hrCtx.closePath();
    hrCtx.fillStyle = grad;
    hrCtx.fill();
  }

  // Stroke line
  hrCtx.beginPath();
  first = true;
  for (const p of pts) {
    if (p.y === null) { first = true; continue; }
    first ? hrCtx.moveTo(p.x, p.y) : hrCtx.lineTo(p.x, p.y);
    first = false;
  }
  hrCtx.strokeStyle = color;
  hrCtx.lineWidth   = 2;
  hrCtx.lineJoin    = "round";
  hrCtx.lineCap     = "round";
  hrCtx.stroke();

  // Draw dots on each data point
  for (const p of pts) {
    if (p.y === null) continue;
    hrCtx.beginPath();
    hrCtx.arc(p.x, p.y, 2.5, 0, Math.PI * 2);
    hrCtx.fillStyle = color;
    hrCtx.fill();
  }
}

window.addEventListener("resize", drawHrChart);

// ── Connection state UI ────────────────────────────────────────
function setConnectionState(state) {
  connBadge.className = `badge badge-${state}`;
  const labels = {
    connected:    "Connected",
    connecting:   "Connecting",
    disconnected: "Disconnected",
    error:        "Error",
  };
  connLabel.textContent = labels[state] ?? state;
}

// ── Update sensor panels ───────────────────────────────────────
function updateData(d) {
  // Show grid, hide waiting cards
  waitConn.classList.add("hidden");
  waitData.classList.add("hidden");
  sensorGrid.classList.remove("hidden");

  // ── Heart Rate ─────────────────────────────────────────────
  const displayBpm = d.finger && d.bpm > 0;
  const isAbnormal = d.finger && d.bpm > 0 && (d.bpm > 120 || d.bpm < 50);

  const bpmEl = $("hr-bpm");
  bpmEl.textContent = displayBpm ? d.bpm : "--";
  bpmEl.className = "hr-value " + (isAbnormal ? "stress-high" : displayBpm ? "stress-low" : "muted");

  $("hr-status").textContent = !d.finger
    ? "No finger"
    : isAbnormal ? "Abnormal range" : "Normal";

  $("hr-icon").style.color = isAbnormal
    ? "var(--red)"
    : d.finger ? "var(--green)" : "var(--fg-muted)";

  // Full-width graph — visible only when finger is on the sensor
  if (d.finger) {
    hrGraphSection.classList.remove("hidden");
    const hrVal = displayBpm ? d.bpm : 0;
    hrHistory.push(hrVal);
    if (hrHistory.length > MAX_HR_HISTORY) hrHistory.shift();
    drawHrChart();
  } else {
    hrGraphSection.classList.add("hidden");
  }

  // ── Stress ─────────────────────────────────────────────────
  const stressEl = $("stress-level");
  stressEl.textContent = d.stress ?? "--";
  stressEl.className = "stress-big " + {
    "Low":      "stress-low",
    "Moderate": "stress-moderate",
    "High":     "stress-high",
  }[d.stress] ?? "muted";

  $("gsr-value").textContent = d.gsr != null
    ? `${Number(d.gsr).toFixed(1)} µS`
    : "-- µS";

  // ── Sleep ──────────────────────────────────────────────────
  const sleepDescriptions = {
    "Deep Sleep":  "Minimal movement detected",
    "Light Sleep": "Low movement detected",
    "Restless":    "Frequent movement detected",
    "Awake":       "Active movement",
  };
  $("sleep-desc").textContent = sleepDescriptions[d.sleep] ?? "Monitoring";

  const sleepBadge = $("sleep-badge");
  sleepBadge.textContent = d.sleep ?? "--";
  sleepBadge.className = "badge mt-sm " + ({
    "Deep Sleep":  "sleep-deep",
    "Light Sleep": "sleep-light",
    "Restless":    "sleep-restless",
    "Awake":       "sleep-awake",
  }[d.sleep] ?? "badge-outline");

  // ── IMU ────────────────────────────────────────────────────
  $("ax-val").textContent  = d.ax != null ? `${Number(d.ax).toFixed(3)}g` : "--";
  $("ay-val").textContent  = d.ay != null ? `${Number(d.ay).toFixed(3)}g` : "--";
  $("az-val").textContent  = d.az != null ? `${Number(d.az).toFixed(3)}g` : "--";
  $("temp-val").textContent = d.temp != null ? `${Number(d.temp).toFixed(1)} °C` : "-- °C";

  // ── Emergency ─────────────────────────────────────────────
  emergency.classList.toggle("hidden", !d.emergency);

  // ── Alarms & Mic State ─────────────────────────────────────
  const alarmOnBtn = $("alarm-on-btn");
  const alarmOffBtn = $("alarm-off-btn");
  if (alarmOnBtn && alarmOffBtn) {
    const disableAlarms = d.emergency || d.mic_active;
    alarmOnBtn.disabled = disableAlarms;
    alarmOffBtn.disabled = disableAlarms;
    alarmOnBtn.style.opacity = disableAlarms ? "0.5" : "1";
    alarmOffBtn.style.opacity = disableAlarms ? "0.5" : "1";
    alarmOnBtn.style.cursor = disableAlarms ? "not-allowed" : "pointer";
    alarmOffBtn.style.cursor = disableAlarms ? "not-allowed" : "pointer";
    alarmOnBtn.title = disableAlarms ? "Disabled during emergency or active mic" : "";
    alarmOffBtn.title = disableAlarms ? "Disabled during emergency or active mic" : "";
  }

  // ── Breathing dot ──────────────────────────────────────────
  $("breathing-dot").classList.toggle("hidden", !d.breathing);

  // ── Device info ────────────────────────────────────────────
  if (d.heap != null) {
    const kb  = (d.heap / 1024).toFixed(1);
    const pct = Math.min(100, (d.heap / (512 * 1024)) * 100);
    $("heap-val").textContent = `${kb} KB`;
    $("heap-bar").style.width = `${pct}%`;
  }

  if (d.uptime != null) {
    $("uptime-val").textContent = formatUptime(d.uptime);
  }

  $("voice-val").textContent = d.voice || "—";

  // ── Camera: auto-open when firmware signals, auto-close when signal ends ──
  // Two-layer close guarantee:
  //   1. Strict SSE check: d.camOpen === true, so undefined/false both trigger close path.
  //   2. Backup setTimeout: if SSE camOpen=false packet is ever lost, the timer closes
  //      the stream at 22s regardless (firmware window is 20s).
  if (d.camOpen === true) {
    if ($("cam-open").classList.contains("hidden")) {
      openCamera($("esp-ip").value || d.ip);
      camAutoOpened = true;
      // Backup timer — clear any previous one first
      if (window._camCloseTimer) clearTimeout(window._camCloseTimer);
      window._camCloseTimer = setTimeout(() => {
        if (camAutoOpened) { closeCamera(); camAutoOpened = false; }
        window._camCloseTimer = null;
      }, 22000);
    }
  } else if (camAutoOpened) {
    if (window._camCloseTimer) { clearTimeout(window._camCloseTimer); window._camCloseTimer = null; }
    closeCamera();
    camAutoOpened = false;
  }

  // ── Auto-update IP input ───────────────────────────────────
  if (d.ip && d.ip !== "0.0.0.0") {
    const ipInput = $("esp-ip");
    if (!ipInput.matches(":focus")) {
      ipInput.value = d.ip;
    }
  }
}

// ── Uptime formatter ───────────────────────────────────────────
function formatUptime(secs) {
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  const s = secs % 60;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

// ── Camera ─────────────────────────────────────────────────────
// IMPORTANT: never use outerHTML to replace #cam-img — that destroys
// the element and breaks all subsequent open/close calls until page refresh.
// Instead, show a sibling error overlay and keep the <img> in place.
function openCamera(ip) {
  const espIp = ip || $("esp-ip").value;
  const url   = `http://${espIp}:81/stream`;

  const img = $("cam-img");
  const err = $("cam-error");

  // Reset any previous error state
  if (err)  err.classList.add("hidden");
  img.style.display = "block";
  img.src = url;

  img.onerror = () => {
    img.style.display = "none";
    if (err) {
      err.textContent = `Cannot reach camera at ${espIp}:81`;
      err.classList.remove("hidden");
    }
  };
  img.onload = () => {
    img.style.display = "block";
    if (err) err.classList.add("hidden");
  };

  $("cam-closed").classList.add("hidden");
  $("cam-open").classList.remove("hidden");
}

function closeCamera() {
  const img = $("cam-img");
  img.src           = "";
  img.onerror       = null;
  img.onload        = null;
  img.style.display = "block";
  const err = $("cam-error");
  if (err) err.classList.add("hidden");
  $("cam-closed").classList.remove("hidden");
  $("cam-open").classList.add("hidden");
  camAutoOpened = false;
}

// ── Send command ───────────────────────────────────────────────
async function sendCmd(cmd) {
  try {
    const res = await fetch("/cmd", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ cmd }),
    });
    if (!res.ok) console.warn("[CMD] Server error:", await res.text());
    else console.log("[CMD] Sent:", cmd);
  } catch(e) {
    console.error("[CMD] Fetch failed:", e);
  }
}

// ── SSE Connection ─────────────────────────────────────────────
function connectSSE() {
  setConnectionState("connecting");
  const es = new EventSource("/stream");

  es.onopen = () => {
    connected = true;
    setConnectionState("connected");
    console.log("[SSE] Connected");
    // Show 'waiting for data' if no grid yet
    if (sensorGrid.classList.contains("hidden")) {
      waitConn.classList.add("hidden");
      waitData.classList.remove("hidden");
    }
  };

  es.addEventListener("data", e => {
    try { updateData(JSON.parse(e.data)); }
    catch(err) { console.error("[SSE] data parse error:", err); }
  });

  es.addEventListener("alert", e => {
    try {
      const msg = JSON.parse(e.data);
      emergency.classList.toggle("hidden", !msg.emergency);
    } catch {}
  });

  es.onerror = () => {
    connected = false;
    setConnectionState("disconnected");
    console.warn("[SSE] Disconnected — browser will retry automatically");
  };
}

// ── Boot ───────────────────────────────────────────────────────
connectSSE();
