// =============================================================
// Web Server — AsyncWebServer Implementation
// =============================================================
#include "web_server.h"
#include "../config.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);
static DashboardState* dashState = nullptr;

// ============ Embedded Dashboard HTML ============
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>M.I.N.D Companion</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0a0a1a;color:#e0e0e0;}
h1{text-align:center;padding:20px 0;font-size:28px;
   background:linear-gradient(135deg,#1a0033,#0d001a);
   border-bottom:2px solid #3498db;}
.camera-wrap{max-width:640px;margin:15px auto;padding:0 15px;display:none;}
.camera-box{border:3px solid #3498db;border-radius:12px;overflow:hidden;background:#000;}
.camera-box img{width:100%;display:block;}
.cam-title{color:#3498db;text-align:center;margin-bottom:8px;font-size:18px;}
.cam-countdown{text-align:center;color:#aaa;font-size:13px;margin-top:6px;}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:15px;padding:15px;}
.card{background:linear-gradient(145deg,#1b0d35,#120828);padding:18px;border-radius:14px;
      text-align:center;box-shadow:0 4px 20px rgba(0,0,0,0.5);border:1px solid #222;}
.card h3{font-size:14px;color:#aaa;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px;}
.value{font-size:32px;font-weight:bold;margin:5px 0;}
.sub{font-size:14px;color:#888;margin-top:5px;}
.badge{display:inline-block;padding:4px 12px;border-radius:20px;font-size:13px;font-weight:bold;}
.low{background:#27ae60;color:#fff;}
.moderate{background:#f1c40f;color:#000;}
.high{background:#e74c3c;color:#fff;}
.emergency-banner{display:none;background:#e74c3c;color:#fff;text-align:center;
                  padding:12px;font-size:20px;font-weight:bold;animation:pulse 1s infinite;}
@keyframes pulse{0%,100%{opacity:1;}50%{opacity:0.6;}}
.btn{padding:10px 20px;border:none;border-radius:8px;font-size:14px;margin:4px;cursor:pointer;
     color:#fff;transition:transform 0.1s;}
.btn:active{transform:scale(0.95);}
.btn-on{background:#27ae60;} .btn-off{background:#c0392b;}
footer{text-align:center;padding:15px;color:#555;font-size:12px;}
</style>
</head>
<body>

<div class="emergency-banner" id="emergBanner">⚠ EMERGENCY ALERT — CHILD NEEDS HELP ⚠</div>
<h1>🧠 M.I.N.D Companion</h1>

<div class="camera-wrap" id="cameraWrap">
  <div class="cam-title">📷 Live Camera Feed</div>
  <div class="camera-box">
    <img id="camImg" src="" alt="Camera Stream">
  </div>
  <div class="cam-countdown" id="camCountdown"></div>
</div>

<div class="grid">

  <div class="card">
    <h3>❤️ Heart Rate</h3>
    <div class="value" id="hr">--</div>
    <div class="sub" id="hrStatus">No Finger</div>
  </div>

  <div class="card">
    <h3>🧪 GSR / Stress</h3>
    <div class="value" id="gsr">--</div>
    <div id="stressBadge"><span class="badge low">--</span></div>
  </div>

  <div class="card">
    <h3>😴 Sleep Status</h3>
    <div class="value" id="sleep" style="color:#f1c40f;">--</div>
  </div>

  <div class="card">
    <h3>📐 Motion (MPU)</h3>
    <div class="sub" id="mpu">X:-- Y:-- Z:--</div>
    <div class="sub" id="temp">Temp: --°C</div>
  </div>

  <div class="card">
    <h3>🚨 Emergency</h3>
    <div class="value" id="emergency" style="color:#2ecc71;">OK</div>
    <button id="clearEmergBtn" class="btn btn-off"
      style="display:none;margin-top:8px;"
      onclick="clearEmergency()">✕ Dismiss</button>
  </div>

  <div class="card">
    <h3>🎤 Last Voice Command</h3>
    <div class="sub" id="voice">--</div>
  </div>

  <div class="card">
    <h3>🌬️ Breathing LED</h3>
    <div class="sub" id="breathing">Inactive</div>
    <button class="btn btn-on" onclick="fetch('/api/breathe')">Start</button>
  </div>

  <div class="card">
    <h3>� Speaker Alarm</h3>
    <button class="btn btn-on" onclick="fetch('/api/alarm?state=on')">ON</button>
    <button class="btn btn-off" onclick="fetch('/api/alarm?state=off')">OFF</button>
  </div>

  <div class="card">
    <h3>📳 Vibration</h3>
    <button class="btn btn-on" onclick="triggerVibrate()">Pulse</button>
  </div>

</div>

<footer>M.I.N.D Companion • ESP32-S3 • Dashboard v2.1</footer>

<script>
// ── Sensor update ─────────────────────────────────────────────
function update(){
  fetch('/api/data').then(r=>r.json()).then(d=>{
    document.getElementById('hr').textContent=d.bpm>0?d.bpm+' bpm':'--';
    document.getElementById('hrStatus').textContent=d.finger?'Finger Detected':'No Finger';
    document.getElementById('hr').style.color=d.finger?'#e74c3c':'#888';

    document.getElementById('gsr').textContent=Math.round(d.gsr)+' Ω';
    let sl=d.stress.toLowerCase();
    let cls=sl==='high'?'high':sl==='moderate'?'moderate':'low';
    document.getElementById('stressBadge').innerHTML=
      '<span class="badge '+cls+'">'+d.stress+'</span>';

    document.getElementById('sleep').textContent=d.sleep||'--';
    document.getElementById('mpu').textContent=
      'X:'+d.ax.toFixed(1)+' Y:'+d.ay.toFixed(1)+' Z:'+d.az.toFixed(1);
    document.getElementById('temp').textContent='Temp: '+d.temp.toFixed(1)+'°C';

    let em=d.emergency;
    document.getElementById('emergency').textContent=em?'ALERT':'OK';
    document.getElementById('emergency').style.color=em?'#e74c3c':'#2ecc71';
    document.getElementById('emergBanner').style.display=em?'block':'none';
    document.getElementById('clearEmergBtn').style.display=em?'inline-block':'none';

    document.getElementById('voice').textContent=d.voice||'--';
    document.getElementById('breathing').textContent=d.breathing?'Active ✨':'Inactive';

    // If server signals camera should open (awake nudge) and it's not already open, open it
    if (d.camOpen && !camTimer) { openCamera(20); }
  }).catch(e=>console.error(e));
}
setInterval(update, 3000);   // 3-second dashboard refresh (less TCP pressure)
update();

function clearEmergency(){
  fetch('/api/emergency/clear').then(()=>update());
}

// ── Camera open/close for vibration pulse ─────────────────────
let camTimer    = null;
let camInterval = null;

function openCamera(seconds) {
  const wrap      = document.getElementById('cameraWrap');
  const img       = document.getElementById('camImg');
  const countdown = document.getElementById('camCountdown');

  // Clear any existing timer
  if (camTimer)    { clearTimeout(camTimer);   camTimer    = null; }
  if (camInterval) { clearInterval(camInterval); camInterval = null; }

  // Start stream and show section
  img.src = 'http://' + window.location.hostname + ':81/stream';
  wrap.style.display = 'block';

  // Countdown display
  let remaining = seconds;
  countdown.textContent = 'Closing in ' + remaining + 's';
  camInterval = setInterval(function(){
    remaining--;
    if (remaining > 0) {
      countdown.textContent = 'Closing in ' + remaining + 's';
    } else {
      clearInterval(camInterval);
      camInterval = null;
    }
  }, 1000);

  // Auto-close after `seconds`
  camTimer = setTimeout(function(){
    img.src = '';                        // stop MJPEG stream
    wrap.style.display = 'none';
    countdown.textContent = '';
    camTimer = null;
  }, seconds * 1000);
}

function triggerVibrate(){
  fetch('/api/vibrate').then(()=>{ openCamera(20); });
}

</script>
</body>
</html>
)rawliteral";

// ============ API Handlers ============

static void handleData(AsyncWebServerRequest* request) {
    if (!dashState) { request->send(500); return; }

    JsonDocument doc;
    doc["bpm"]       = dashState->heartBPM;
    doc["finger"]    = dashState->fingerPresent;
    doc["gsr"]       = dashState->gsrValue;
    doc["stress"]    = dashState->stressLevel;
    doc["sleep"]     = dashState->sleepQuality;
    doc["emergency"] = dashState->emergencyActive;
    doc["ax"]        = dashState->accelX;
    doc["ay"]        = dashState->accelY;
    doc["az"]        = dashState->accelZ;
    doc["temp"]      = dashState->temperature;
    doc["voice"]     = dashState->lastVoiceCommand;
    doc["breathing"] = dashState->breathingActive;
    doc["camOpen"]   = dashState->cameraOpen;
    dashState->cameraOpen = false;   // one-shot: clear after dashboard reads it

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

// ============ External action callbacks (set from main.cpp) ============
static void (*breatheCallback)()        = nullptr;
static void (*alarmOnCallback)()        = nullptr;
static void (*alarmOffCallback)()       = nullptr;
static void (*vibrateCallback)()        = nullptr;
static void (*clearEmergencyCallback)() = nullptr;

void webServerInit(DashboardState* state) {
    dashState = state;

    // Dashboard page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", DASHBOARD_HTML);
    });

    // JSON data endpoint
    server.on("/api/data", HTTP_GET, handleData);

    // Control endpoints
    server.on("/api/breathe", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (breatheCallback) breatheCallback();
        req->send(200, "text/plain", "OK");
    });

    server.on("/api/alarm", HTTP_GET, [](AsyncWebServerRequest* req) {
        String s = req->hasParam("state") ? req->getParam("state")->value() : "";
        if (s == "on" && alarmOnCallback)   alarmOnCallback();
        if (s == "off" && alarmOffCallback) alarmOffCallback();
        req->send(200, "text/plain", "OK");
    });

    server.on("/api/vibrate", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (vibrateCallback) vibrateCallback();
        req->send(200, "text/plain", "OK");
    });

    server.on("/api/emergency/clear", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (clearEmergencyCallback) clearEmergencyCallback();
        req->send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println("[WEB] AsyncWebServer started on port 80");
}

void webServerSetCallbacks(
    void (*breathe)(),
    void (*alarmOn)(),
    void (*alarmOff)(),
    void (*vibrate)(),
    void (*clearEmergency)()
) {
    breatheCallback        = breathe;
    alarmOnCallback        = alarmOn;
    alarmOffCallback       = alarmOff;
    vibrateCallback        = vibrate;
    clearEmergencyCallback = clearEmergency;
}
