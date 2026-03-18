/**
 * M.I.N.D. Companion Dashboard — Client-Side JS
 * Connects to Flask via Server-Sent Events (SSE).
 * All real-time panel updates happen here.
 */

// ── State ──────────────────────────────────────────────────────
const MAX_HISTORY      = 60;   // HR and Stress: 60 seconds
const MAX_SLEEP_HISTORY = 120; // Sleep state: 120 seconds

let hrHistory     = [];
let sleepHistory  = [];
let stressHistory = [];
let connected     = false;
let camAutoOpened = false;

const SLEEP_MAP  = { "Deep Sleep": 0, "Light Sleep": 1, "Awake": 2 };
const STRESS_MAP = { "Low": 0, "Moderate": 1, "High": 2 };

// ── DOM refs ───────────────────────────────────────────────────
const $ = id => document.getElementById(id);

const connBadge   = $("conn-badge");
const connLabel   = $("conn-label");
const waitConn    = $("wait-conn");
const waitData    = $("wait-data");
const sensorGrid  = $("sensor-grid");
const emergency   = $("emergency-banner");

// ── Generic Axis Chart Renderer ─────────────────────────────────
function drawAxisChart(canvas, data, config) {
  const ctx = canvas.getContext("2d");
  const dpr = window.devicePixelRatio || 1;
  const wrap = canvas.parentElement;
  const isShort = wrap && wrap.classList.contains("chart-wrap-short");
  const isSquare = wrap && wrap.classList.contains("chart-wrap-square");
  const size = wrap ? Math.min(wrap.offsetWidth || 400, wrap.offsetHeight || 400) : 400;
  const w = (isShort || isSquare) && wrap ? size : (wrap ? (wrap.offsetWidth || 600) : 600);
  const h = (isShort || isSquare) && wrap ? size : (config.height || 140);

  canvas.width  = w * dpr;
  canvas.height = h * dpr;
  canvas.style.width  = w + "px";
  canvas.style.height  = h + "px";
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, w, h);

  const left   = config.leftPad || 52;
  const right  = 16;
  const top    = 14;
  const bottom = 24;
  const plotW  = w - left - right;
  const plotH  = h - top - bottom;

  const color   = config.color;
  const yLabels = config.yLabels;
  const yMin    = config.yMin;
  const yMax    = config.yMax;
  const yRange  = yMax - yMin || 1;

  const toY = v => top + plotH - ((v - yMin) / yRange) * plotH;
  const toX = i => left + (data.length > 1 ? (i / (data.length - 1)) * plotW : 0);

  // Grid lines + Y-axis labels
  ctx.font = "10px 'JetBrains Mono', monospace";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";

  for (const yl of yLabels) {
    const y = toY(yl.value);
    ctx.strokeStyle = "rgba(255,255,255,0.05)";
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(w - right, y);
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = "rgba(161,161,170,0.7)";
    ctx.fillText(yl.label, left - 8, y);
  }

  // Axes
  ctx.strokeStyle = "rgba(255,255,255,0.2)";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(left, top);
  ctx.lineTo(left, h - bottom);
  ctx.lineTo(w - right, h - bottom);
  ctx.stroke();

  // X-axis ticks (every 10 samples)
  ctx.font = "9px 'JetBrains Mono', monospace";
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  ctx.fillStyle = "rgba(161,161,170,0.5)";
  if (data.length > 1) {
    const step = Math.max(10, Math.ceil(data.length / 6));
    for (let i = 0; i < data.length; i += step) {
      const x = toX(i);
      ctx.beginPath();
      ctx.moveTo(x, h - bottom);
      ctx.lineTo(x, h - bottom + 4);
      ctx.strokeStyle = "rgba(255,255,255,0.15)";
      ctx.stroke();
      ctx.fillText(`${i}s`, x, h - bottom + 5);
    }
  }

  if (data.length === 0) return;

  const validData = data.filter(v => v !== null);
  if (validData.length === 0) return;

  const pts = data.map((v, i) => ({
    x: toX(i),
    y: v !== null ? toY(v) : null,
  }));

  const grad = ctx.createLinearGradient(0, top, 0, h - bottom);
  grad.addColorStop(0, color + "30");
  grad.addColorStop(1, color + "05");

  if (config.stepped) {
    // Stepped line for categorical data
    let started = false;
    let firstX = 0, lastX = 0, lastPY = 0;

    // Fill
    ctx.beginPath();
    for (let i = 0; i < pts.length; i++) {
      if (pts[i].y === null) continue;
      if (!started) {
        ctx.moveTo(pts[i].x, pts[i].y);
        firstX = pts[i].x;
        started = true;
      } else {
        ctx.lineTo(pts[i].x, lastPY);
        ctx.lineTo(pts[i].x, pts[i].y);
      }
      lastX = pts[i].x;
      lastPY = pts[i].y;
    }
    if (started) {
      ctx.lineTo(lastX, h - bottom);
      ctx.lineTo(firstX, h - bottom);
      ctx.closePath();
      ctx.fillStyle = grad;
      ctx.fill();
    }

    // Stroke
    ctx.beginPath();
    started = false;
    for (let i = 0; i < pts.length; i++) {
      if (pts[i].y === null) continue;
      if (!started) {
        ctx.moveTo(pts[i].x, pts[i].y);
        started = true;
      } else {
        ctx.lineTo(pts[i].x, lastPY);
        ctx.lineTo(pts[i].x, pts[i].y);
      }
      lastPY = pts[i].y;
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.lineJoin = "round";
    ctx.stroke();

    // Dots at each data point
    for (const p of pts) {
      if (p.y === null) continue;
      ctx.beginPath();
      ctx.arc(p.x, p.y, 3, 0, Math.PI * 2);
      ctx.fillStyle = color;
      ctx.fill();
    }

  } else {
    // Smooth line for numeric data

    // Fill
    ctx.beginPath();
    let first = true;
    for (const p of pts) {
      if (p.y === null) { first = true; continue; }
      first ? ctx.moveTo(p.x, p.y) : ctx.lineTo(p.x, p.y);
      first = false;
    }
    const lastValid  = [...pts].reverse().find(p => p.y !== null);
    const firstValid = pts.find(p => p.y !== null);
    if (lastValid && firstValid) {
      ctx.lineTo(lastValid.x, h - bottom);
      ctx.lineTo(firstValid.x, h - bottom);
      ctx.closePath();
      ctx.fillStyle = grad;
      ctx.fill();
    }

    // Stroke
    ctx.beginPath();
    first = true;
    for (const p of pts) {
      if (p.y === null) { first = true; continue; }
      first ? ctx.moveTo(p.x, p.y) : ctx.lineTo(p.x, p.y);
      first = false;
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.lineJoin = "round";
    ctx.lineCap  = "round";
    ctx.stroke();

    // Dots
    for (const p of pts) {
      if (p.y === null) continue;
      ctx.beginPath();
      ctx.arc(p.x, p.y, 2.5, 0, Math.PI * 2);
      ctx.fillStyle = color;
      ctx.fill();
    }
  }
}

// ── Chart draw wrappers ─────────────────────────────────────────

function drawHrChart() {
  const vals = hrHistory.filter(v => v > 0);
  let yMin, yMax, labels;

  if (vals.length > 0) {
    yMin = Math.max(0, Math.min(...vals) - 10);
    yMax = Math.max(...vals) + 10;
    const mid = Math.round((yMin + yMax) / 2);
    labels = [
      { value: Math.round(yMin), label: `${Math.round(yMin)}` },
      { value: mid,              label: `${mid}` },
      { value: Math.round(yMax), label: `${Math.round(yMax)}` },
    ];
  } else {
    yMin = 40; yMax = 160;
    labels = [
      { value: 40,  label: "40" },
      { value: 80,  label: "80" },
      { value: 120, label: "120" },
      { value: 160, label: "160" },
    ];
  }

  const isAbnormal = vals.some(v => v > 120 || v < 50);
  drawAxisChart($("hr-chart"), hrHistory, {
    height:  150,
    color:   isAbnormal ? "#f87171" : "#4ade80",
    yMin,
    yMax,
    yLabels: labels,
    stepped: false,
    leftPad: 48,
  });
}

function drawSleepChart() {
  drawAxisChart($("sleep-chart"), sleepHistory, {
    height:  130,
    color:   "#60a5fa",
    yMin:    -0.3,
    yMax:    2.3,
    yLabels: [
      { value: 0, label: "Deep" },
      { value: 1, label: "Light" },
      { value: 2, label: "Awake" },
    ],
    stepped: true,
    leftPad: 52,
  });
}

function drawStressChart() {
  drawAxisChart($("stress-chart"), stressHistory, {
    height:  130,
    color:   "#facc15",
    yMin:    -0.3,
    yMax:    2.3,
    yLabels: [
      { value: 0, label: "Low" },
      { value: 1, label: "Mid" },
      { value: 2, label: "High" },
    ],
    stepped: true,
    leftPad: 52,
  });
}

function redrawAllCharts() {
  drawHrChart();
  drawSleepChart();
  drawStressChart();
}

window.addEventListener("resize", redrawAllCharts);

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

  // Finger badge on graph
  $("finger-badge").classList.toggle("hidden", !d.finger);

  // Record HR (push 0 when no finger so graph shows gap as null)
  const hrVal = displayBpm ? d.bpm : null;
  hrHistory.push(hrVal);
  if (hrHistory.length > MAX_HISTORY) hrHistory.shift();
  drawHrChart();

  // ── Stress ─────────────────────────────────────────────────
  const stressEl = $("stress-level");
  stressEl.textContent = d.stress ?? "--";
  const isWearStrap = d.stress && String(d.stress).toLowerCase().includes("please wear");
  const stateClass = {
    "Low":      "stress-low",
    "Moderate": "stress-moderate",
    "High":     "stress-high",
  }[d.stress] ?? "muted";
  stressEl.className = "stress-big " + (isWearStrap ? "stress-wear-strap " : "") + stateClass;

  $("gsr-value").textContent = d.gsr != null
    ? `${Number(d.gsr).toFixed(1)} µS`
    : "-- µS";

  // Record stress (only Low/Moderate/High)
  if (d.stress in STRESS_MAP) {
    stressHistory.push(STRESS_MAP[d.stress]);
    if (stressHistory.length > MAX_HISTORY) stressHistory.shift();
    drawStressChart();
  }

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

  // Record sleep (only Deep/Light/Awake) — keep 120 s for Sleep graph
  if (d.sleep in SLEEP_MAP) {
    sleepHistory.push(SLEEP_MAP[d.sleep]);
    if (sleepHistory.length > MAX_SLEEP_HISTORY) sleepHistory.shift();
    drawSleepChart();
  }

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

  // ── Camera auto-open/close ─────────────────────────────────
  if (d.camOpen === true) {
    if ($("cam-open").classList.contains("hidden")) {
      openCamera($("esp-ip").value || d.ip);
      camAutoOpened = true;
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
function openCamera(ip) {
  const espIp = ip || $("esp-ip").value;
  const url   = `http://${espIp}:81/stream`;

  const img = $("cam-img");
  const err = $("cam-error");

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
redrawAllCharts();
