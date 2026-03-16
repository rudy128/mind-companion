/**
 * M.I.N.D. Companion Dashboard — Client-Side JS
 * Connects to Flask via Server-Sent Events (SSE).
 * All real-time panel updates happen here.
 */

// ── State ──────────────────────────────────────────────────────
const MAX_HR_HISTORY = 50;
const MAX_LOGS       = 200;

let hrHistory    = [];
let allLogs      = [];
let logFilter    = "ALL";
let audioBlob    = null;
let audioUrl     = null;
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

// ── HR chart (plain Canvas) ────────────────────────────────────
const hrCanvas = $("hr-chart");
const hrCtx    = hrCanvas.getContext("2d");

function drawHrChart() {
  const w = hrCanvas.offsetWidth || 280;
  const h = 72;
  hrCanvas.width  = w;
  hrCanvas.height = h;

  hrCtx.clearRect(0, 0, w, h);
  if (hrHistory.length < 2) return;

  const vals  = hrHistory.filter(v => v > 0);
  if (vals.length === 0) return;

  const min = Math.max(0, Math.min(...vals) - 5);
  const max = Math.max(...vals) + 5;
  const range = max - min || 1;

  const isAbnormal = vals.some(v => v > 120 || v < 50);
  const color = isAbnormal ? "#f87171" : "#4ade80";

  const pts = hrHistory.map((v, i) => ({
    x: (i / (hrHistory.length - 1)) * w,
    y: v > 0 ? h - ((v - min) / range) * h * .85 - h * .075 : null,
  }));

  // Fill gradient
  const grad = hrCtx.createLinearGradient(0, 0, 0, h);
  grad.addColorStop(0,   color + "44");
  grad.addColorStop(1,   color + "03");

  hrCtx.beginPath();
  let first = true;
  for (const p of pts) {
    if (p.y === null) { first = true; continue; }
    first ? hrCtx.moveTo(p.x, p.y) : hrCtx.lineTo(p.x, p.y);
    first = false;
  }
  // Close fill path down to baseline
  const lastValid = [...pts].reverse().find(p => p.y !== null);
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
  hrCtx.lineWidth   = 1.5;
  hrCtx.lineJoin    = "round";
  hrCtx.stroke();
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
    : "var(--fg-muted)";

  const hrVal = displayBpm ? d.bpm : 0;
  if (hrHistory.length === 0 || hrHistory[hrHistory.length - 1] !== hrVal) {
    hrHistory.push(hrVal);
    if (hrHistory.length > MAX_HR_HISTORY) hrHistory.shift();
  }
  drawHrChart();

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
  // camAutoOpened flag ensures we never auto-close a stream the user opened manually.
  if (d.camOpen) {
    if ($("#cam-open").classList.contains("hidden")) {
      openCamera($("#esp-ip").value || d.ip);
      camAutoOpened = true;
    }
  } else if (camAutoOpened) {
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

// ── Logs ───────────────────────────────────────────────────────
function appendLog(entry) {
  allLogs.unshift(entry);
  if (allLogs.length > MAX_LOGS) allLogs.pop();
  renderLogs();
}

function renderLogs() {
  const area = $("log-area");
  const filtered = logFilter === "ALL"
    ? allLogs
    : allLogs.filter(e => e.level === logFilter);

  $("log-count").textContent = `${filtered.length} entries`;

  if (filtered.length === 0) {
    area.innerHTML = `<p class="muted text-sm">No ${logFilter === "ALL" ? "" : logFilter + " "}logs yet...</p>`;
    return;
  }

  area.innerHTML = filtered.map(e => `
    <div class="log-entry">
      <span class="log-ts">${(e.ts / 1000).toFixed(1)}s</span>
      <span class="log-level lv-${e.level.toLowerCase()}">${e.level}</span>
      <span class="log-tag">[${e.tag}]</span>
      <span class="log-msg">${escHtml(e.msg)}</span>
    </div>`
  ).join("");
}

function clearLogs() {
  allLogs = [];
  renderLogs();
}

function escHtml(s) {
  return String(s)
    .replace(/&/g,"&amp;")
    .replace(/</g,"&lt;")
    .replace(/>/g,"&gt;");
}

// Filter buttons
document.querySelectorAll(".filter-btn").forEach(btn => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".filter-btn").forEach(b => b.classList.remove("active"));
    btn.classList.add("active");
    logFilter = btn.dataset.level;
    renderLogs();
  });
});

// ── Audio ──────────────────────────────────────────────────────
function handleAudio(base64Raw) {
  try {
    const clean   = base64Raw.replace(/[^A-Za-z0-9+/=]/g, "");
    const padded  = clean.padEnd(clean.length + (4 - clean.length % 4) % 4, "=");
    const binary  = atob(padded);
    const bytes   = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);

    if (audioUrl) URL.revokeObjectURL(audioUrl);
    audioBlob = new Blob([bytes], { type: "audio/wav" });
    audioUrl  = URL.createObjectURL(audioBlob);

    const audioEl = $("audio-el");
    audioEl.src   = audioUrl;

    $("audio-empty").classList.add("hidden");
    $("audio-player").classList.remove("hidden");
    $("audio-size").textContent =
      `${Math.round(base64Raw.length / 1024)} KB base64 → ~${Math.round(base64Raw.length * 0.75 / 1024)} KB WAV`;
  } catch(e) {
    console.error("[Audio] Decode failed:", e);
  }
}

function clearAudio() {
  if (audioUrl) URL.revokeObjectURL(audioUrl);
  audioUrl = audioBlob = null;
  $("audio-el").src = "";
  $("audio-empty").classList.remove("hidden");
  $("audio-player").classList.add("hidden");
  $("audio-size").textContent = "";
}

function downloadAudio() {
  if (!audioUrl) return;
  const a = document.createElement("a");
  a.href = audioUrl;
  a.download = `mic-recording-${Date.now()}.wav`;
  a.click();
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

  es.addEventListener("log", e => {
    try { appendLog(JSON.parse(e.data)); }
    catch {}
  });

  es.addEventListener("alert", e => {
    try {
      const msg = JSON.parse(e.data);
      emergency.classList.toggle("hidden", !msg.emergency);
    } catch {}
  });

  es.addEventListener("audio", e => {
    handleAudio(e.data);
  });

  es.addEventListener("ai_response", e => {
    const aiSection  = $("ai-section");
    const aiResponse = $("ai-response");
    aiSection.classList.remove("hidden");
    aiResponse.textContent = e.data;
  });

  es.onerror = () => {
    connected = false;
    setConnectionState("disconnected");
    console.warn("[SSE] Disconnected — browser will retry automatically");
  };
}

// ── Boot ───────────────────────────────────────────────────────
connectSSE();
