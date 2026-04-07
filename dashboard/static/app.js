/**
 * M.I.N.D. Companion Dashboard — Client-Side JS
 * Connects to Flask via Server-Sent Events (SSE).
 * All real-time panel updates happen here.
 */

// ── State ──────────────────────────────────────────────────────
const BPM_OFFSET       = 28;   // Add to BPM from MQTT only (dashboard) to align with natural BPM
// HR / stress / sleep chart series grow without a fixed cap (all samples kept for full history on graph)

let hrHistory     = [];
let sleepHistory  = [];
let stressHistory = [];
let camAutoOpened = false;

const SLEEP_MAP  = { "Deep Sleep": 0, "Light Sleep": 1, "Awake": 2 };
const STRESS_MAP = { "Low": 0, "Moderate": 1, "High": 2 };

const SLEEP_KNOWN = ["Deep Sleep", "Light Sleep", "Restless", "Awake"];

/**
 * Step 1 — Base % from heart rate + GSR (display BPM = raw MQTT bpm + BPM_OFFSET).
 * Table rows 1–10; HR ≥ 150 with Moderate/Low extended to stay consistent with high-HR risk.
 */
function getOverallStressBase(hr, gsrLevel) {
  const gH = gsrLevel === "High";
  const gM = gsrLevel === "Moderate";
  const gL = gsrLevel === "Low";

  if (hr >= 150 && gH) return 100;
  if (hr <= 60 && gH) return 100;
  if (hr >= 121 && hr <= 149 && gH) return 90;
  if (hr <= 60 && gM) return 80;
  if (hr >= 121 && hr <= 149 && gM) return 70;
  if (hr >= 61 && hr <= 120 && gH) return 50;
  if (hr >= 61 && hr <= 120 && gM) return 50;
  if (hr >= 121 && hr <= 149 && gL) return 40;
  if (hr <= 60 && gL) return 40;
  if (hr >= 61 && hr <= 120 && gL) return 10;
  if (hr >= 150 && gM) return 90;
  if (hr >= 150 && gL) return 80;
  return null;
}

/**
 * HR-only base % when GSR is unavailable (e.g. straps off). Mirrors table risk by HR band.
 */
function getOverallStressBaseHrOnly(hr) {
  if (hr >= 150) return 95;
  if (hr >= 121) return 65;
  if (hr >= 61) return 30;
  return 60;
}

/**
 * GSR-only base % when heart rate is unavailable (no finger). Conservative defaults.
 */
function getOverallStressBaseGsrOnly(gsrLevel) {
  if (gsrLevel === "High") return 70;
  if (gsrLevel === "Moderate") return 45;
  if (gsrLevel === "Low") return 20;
  return null;
}

/** Step 2 — Sleep modifier (added to base, final capped at 100%). */
function getSleepModifierPct(sleep) {
  if (sleep === "Deep Sleep") return 25;
  if (sleep === "Restless") return 15;
  if (sleep === "Awake" || sleep === "Light Sleep") return 0;
  return 0;
}

/**
 * Human-readable line from final % (aligned with doc examples and table row names).
 */
function getOverallStressLabelFromFinalPct(pct) {
  if (pct >= 90) return "Attention Needed!";
  if (pct >= 75) return "High";
  if (pct >= 70) return "Elevated";
  if (pct >= 50) return "Ask if child needs anything";
  if (pct >= 40) return "Elevated";
  if (pct >= 20) return "Mild concern";
  return "Child is doing great!";
}

/**
 * Overall stress KPI: prefer full HR+GSR table; else HR-only or GSR-only; sleep modifier; cap 100%.
 */
function getOverallStress(d) {
  if (!d) return { pct: null, label: "—", level: "normal" };

  const hasHR = !!(d.finger && d.bpm != null && d.bpm > 0);
  const hasGSR = !!(d.stress && d.stress in STRESS_MAP);
  const wearStrap = d.stress && String(d.stress).toLowerCase().includes("please wear");
  const noUsableGsr = !hasGSR || wearStrap;

  const hr = hasHR ? Number(d.bpm) + BPM_OFFSET : null;
  const gsr = d.stress;
  const sleep = d.sleep;
  const hasSleep = !!(sleep && SLEEP_KNOWN.includes(sleep));
  const mod = hasSleep ? getSleepModifierPct(sleep) : 0;

  let base = null;

  if (hasHR && hasGSR) {
    base = getOverallStressBase(hr, gsr);
  } else if (hasHR && noUsableGsr) {
    base = getOverallStressBaseHrOnly(hr);
  } else if (hasGSR && !hasHR) {
    base = getOverallStressBaseGsrOnly(gsr);
  }

  if (base == null) {
    const label =
      wearStrap && !hasHR ? (d.stress || "—") : "Not enough data";
    return { pct: null, label, level: "normal" };
  }

  const finalPct = Math.min(100, base + mod);
  const label = getOverallStressLabelFromFinalPct(finalPct);

  let level = "normal";
  if (finalPct >= 90) level = "attention";
  else if (finalPct >= 40) level = "high";

  return { pct: finalPct, label, level };
}

// ── DOM refs ───────────────────────────────────────────────────
const $ = id => document.getElementById(id);

const connBadge   = $("conn-badge");
const connLabel   = $("conn-label");
const waitConn    = $("wait-conn");
const waitData    = $("wait-data");
const sensorGrid  = $("sensor-grid");
const emergency   = $("emergency-banner");

// ── Chart.js line charts (smooth HR, stepped sleep/stress; no markers) ───────
const lineCharts = { hr: null, sleep: null, stress: null };

function seriesToPoints(arr) {
  return arr.map((y, i) => ({
    x: i,
    y: y === null || y === undefined ? null : Number(y),
  }));
}

function xAxisMax(len) {
  return len <= 1 ? 1 : len - 1;
}

function chartJsBaseOptions() {
  return {
    responsive: true,
    maintainAspectRatio: false,
    animation: false,
    parsing: false,
    interaction: { intersect: false, mode: "index" },
    plugins: {
      legend: { display: false },
      tooltip: {
        callbacks: {
          title(items) {
            const t = items[0]?.parsed?.x;
            return t != null ? `t = ${Math.round(t)}s` : "";
          },
        },
      },
    },
    elements: {
      point: { radius: 0, hoverRadius: 0, hitRadius: 0 },
      line: { borderWidth: 2, tension: 0 },
    },
    scales: {
      x: {
        type: "linear",
        min: 0,
        max: 1,
        grid: { color: "rgba(255,255,255,0.06)", tickLength: 0 },
        border: { color: "rgba(255,255,255,0.2)", display: true },
        ticks: {
          color: "rgba(161,161,170,0.75)",
          font: { family: "'JetBrains Mono', monospace", size: 10 },
          maxTicksLimit: 14,
          callback(v) {
            const n = Number(v);
            return Number.isInteger(n) && n % 10 === 0 ? `${n}s` : "";
          },
        },
      },
    },
  };
}

function drawHrChart() {
  const canvas = $("hr-chart");
  if (!canvas || typeof Chart === "undefined") return;

  const vals = hrHistory.filter(v => v > 0);
  const yMin = 0;
  let yMax = 160;
  if (vals.length > 0) {
    yMax = Math.max(100, Math.max(...vals) + 10);
  }
  const latest = vals.length > 0 ? vals[vals.length - 1] : 0;
  const isAbnormal = latest > 120;
  const borderColor = isAbnormal ? "#f87171" : "#4ade80";
  const fillColor = isAbnormal ? "rgba(248,113,113,0.12)" : "rgba(74,222,128,0.12)";
  const data = seriesToPoints(hrHistory);
  const xMax = xAxisMax(data.length);
  const base = chartJsBaseOptions();

  const hrYTickCallback = v => {
    const r = Math.round(Number(v));
    const m = Math.round((yMin + yMax) / 2);
    if (r === 0 || r === m || r === Math.round(yMax)) return String(r);
    return "";
  };

  if (!lineCharts.hr) {
    lineCharts.hr = new Chart(canvas, {
      type: "line",
      data: {
        datasets: [{
          data,
          parsing: false,
          borderColor,
          backgroundColor: fillColor,
          borderWidth: 2,
          tension: 0.4,
          fill: true,
          spanGaps: false,
        }],
      },
      options: {
        ...base,
        elements: {
          ...base.elements,
          point: { radius: 0, hoverRadius: 0, hitRadius: 0 },
        },
        scales: {
          ...base.scales,
          x: { ...base.scales.x, min: 0, max: xMax },
          y: {
            min: yMin,
            max: yMax,
            grid: { color: "rgba(255,255,255,0.06)" },
            border: { color: "rgba(255,255,255,0.2)", display: true },
            ticks: {
              color: "rgba(161,161,170,0.7)",
              font: { family: "'JetBrains Mono', monospace", size: 11 },
              callback: hrYTickCallback,
            },
          },
        },
      },
    });
    return;
  }

  const ds = lineCharts.hr.data.datasets[0];
  ds.data = data;
  ds.borderColor = borderColor;
  ds.backgroundColor = fillColor;
  lineCharts.hr.options.scales.x.min = 0;
  lineCharts.hr.options.scales.x.max = xMax;
  lineCharts.hr.options.scales.y.min = yMin;
  lineCharts.hr.options.scales.y.max = yMax;
  lineCharts.hr.options.scales.y.ticks.callback = hrYTickCallback;
  lineCharts.hr.update("none");
  lineCharts.hr.resize();
}

function drawSleepChart() {
  const canvas = $("sleep-chart");
  if (!canvas || typeof Chart === "undefined") return;

  const data = seriesToPoints(sleepHistory);
  const xMax = xAxisMax(data.length);

  if (!lineCharts.sleep) {
    const base = chartJsBaseOptions();
    lineCharts.sleep = new Chart(canvas, {
      type: "line",
      data: {
        datasets: [{
          data,
          parsing: false,
          borderColor: "#60a5fa",
          backgroundColor: "rgba(96,165,250,0.12)",
          borderWidth: 2,
          tension: 0.4,
          fill: "origin",
          spanGaps: true,
        }],
      },
      options: {
        ...base,
        scales: {
          ...base.scales,
          x: { ...base.scales.x, min: 0, max: xMax },
          y: {
            min: -0.3,
            max: 2.3,
            grid: { color: "rgba(255,255,255,0.06)" },
            border: { color: "rgba(255,255,255,0.2)", display: true },
            afterBuildTicks(axis) {
              axis.ticks = [
                { value: 0 },
                { value: 1 },
                { value: 2 },
              ];
            },
            ticks: {
              color: "rgba(161,161,170,0.7)",
              font: { family: "'JetBrains Mono', monospace", size: 11 },
              callback(v) {
                const n = Math.round(Number(v));
                if (n === 0) return "Deep";
                if (n === 1) return "Light";
                if (n === 2) return "Awake";
                return "";
              },
            },
          },
        },
      },
    });
    return;
  }

  lineCharts.sleep.data.datasets[0].data = data;
  lineCharts.sleep.options.scales.x.min = 0;
  lineCharts.sleep.options.scales.x.max = xMax;
  lineCharts.sleep.update("none");
  lineCharts.sleep.resize();
}

function drawStressChart() {
  const canvas = $("stress-chart");
  if (!canvas || typeof Chart === "undefined") return;

  const data = seriesToPoints(stressHistory);
  const xMax = xAxisMax(data.length);

  if (!lineCharts.stress) {
    const base = chartJsBaseOptions();
    lineCharts.stress = new Chart(canvas, {
      type: "line",
      data: {
        datasets: [{
          data,
          parsing: false,
          borderColor: "#facc15",
          backgroundColor: "rgba(250,204,21,0.12)",
          borderWidth: 2,
          tension: 0.4,
          fill: "origin",
          spanGaps: true,
        }],
      },
      options: {
        ...base,
        scales: {
          ...base.scales,
          x: { ...base.scales.x, min: 0, max: xMax },
          y: {
            min: -0.3,
            max: 2.3,
            grid: { color: "rgba(255,255,255,0.06)" },
            border: { color: "rgba(255,255,255,0.2)", display: true },
            afterBuildTicks(axis) {
              axis.ticks = [
                { value: 0 },
                { value: 1 },
                { value: 2 },
              ];
            },
            ticks: {
              color: "rgba(161,161,170,0.7)",
              font: { family: "'JetBrains Mono', monospace", size: 11 },
              callback(v) {
                const n = Math.round(Number(v));
                if (n === 0) return "Low";
                if (n === 1) return "Mid";
                if (n === 2) return "High";
                return "";
              },
            },
          },
        },
      },
    });
    return;
  }

  lineCharts.stress.data.datasets[0].data = data;
  lineCharts.stress.options.scales.x.min = 0;
  lineCharts.stress.options.scales.x.max = xMax;
  lineCharts.stress.update("none");
  lineCharts.stress.resize();
}

function redrawAllCharts() {
  drawHrChart();
  drawSleepChart();
  drawStressChart();
}

window.addEventListener("resize", () => {
  Object.values(lineCharts).forEach(c => {
    if (c) c.resize();
  });
});

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

  // ── Overall Stress (KPI) ─────────────────────────────────────
  const overall = getOverallStress(d);
  const overallCard = $("overall-stress-card");
  const overallPct = $("overall-stress-pct");
  const overallMsg = $("overall-stress-msg");
  if (overallCard && overallPct && overallMsg) {
    overallPct.textContent = overall.pct != null ? overall.pct : "--";
    overallMsg.textContent = overall.label;
    overallCard.classList.remove(
      "overall-stress-high",
      "overall-stress-attention",
      "overall-stress-moderate",
    );
    if (overall.level === "attention") overallCard.classList.add("overall-stress-attention");
    else if (overall.pct != null && overall.pct >= 40) overallCard.classList.add("overall-stress-high");
    else if (overall.pct != null && overall.pct >= 20) overallCard.classList.add("overall-stress-moderate");
    overallMsg.style.setProperty("font-size", "1rem", "important");
    const msgColor = overall.level === "attention" ? "#f87171"
      : (overall.pct != null && overall.pct >= 40) ? "#facc15"
      : (overall.pct != null && overall.pct >= 20) ? "#22d3ee"
      : "#4ade80";
    overallMsg.style.setProperty("color", msgColor, "important");
  }

  // ── Heart Rate (add BPM_OFFSET only when we have a value from MQTT) ──
  const displayBpm = d.finger && d.bpm != null && d.bpm > 0;
  const bpmDisplay = displayBpm ? (Number(d.bpm) + BPM_OFFSET) : null;
  const isAbnormal = displayBpm && (bpmDisplay > 120 || bpmDisplay < 50);

  const bpmEl = $("hr-bpm");
  bpmEl.textContent = displayBpm ? bpmDisplay : "--";
  bpmEl.className = "hr-value " + (isAbnormal ? "stress-high" : displayBpm ? "stress-low" : "muted");

  $("hr-status").textContent = !d.finger
    ? "No finger"
    : isAbnormal ? "Abnormal range" : "Measuring";

  $("hr-icon").style.color = isAbnormal
    ? "var(--red)"
    : d.finger ? "var(--green)" : "var(--fg-muted)";

  // Finger badge on graph
  $("finger-badge").classList.toggle("hidden", !d.finger);

  // Record HR every tick: 0 when no finger; when from MQTT use adjusted BPM
  const hrVal = displayBpm ? bpmDisplay : 0;
  hrHistory.push(hrVal);
  drawHrChart();

  // ── Stress ─────────────────────────────────────────────────
  const stressEl = $("stress-level");
  const isWearStrap = d.stress && String(d.stress).toLowerCase().includes("please wear");
  stressEl.textContent = isWearStrap ? "Please wear finger straps" : (d.stress ?? "--");
  const stateClass = {
    "Low":      "stress-low",
    "Moderate": "stress-moderate",
    "High":     "stress-high",
  }[d.stress] ?? "muted";
  stressEl.className = "stress-big " + (isWearStrap ? "stress-wear-strap " : "") + stateClass;
  stressEl.style.setProperty("font-size", "1rem", "important");

  $("gsr-value").textContent = d.gsr != null
    ? `${Number(d.gsr).toFixed(1)} Ω`
    : "-- Ω";

  const stressVal = isWearStrap ? 0 : (d.stress in STRESS_MAP ? STRESS_MAP[d.stress] : 0);
  stressHistory.push(stressVal);
  drawStressChart();

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

  // Record sleep (only Deep/Light/Awake mapped values)
  if (d.sleep in SLEEP_MAP) {
    sleepHistory.push(SLEEP_MAP[d.sleep]);
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
    setConnectionState("disconnected");
    console.warn("[SSE] Disconnected — browser will retry automatically");
  };
}

// ── Boot ───────────────────────────────────────────────────────
connectSSE();
redrawAllCharts();
