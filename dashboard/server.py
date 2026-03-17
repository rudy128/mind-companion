"""
M.I.N.D. Companion — Flask Dashboard Server
Bridges MQTT (Mosquitto) ↔ Browser via Server-Sent Events (SSE).

Architecture:
  - paho-mqtt subscribes to mind/* topics on the local broker
  - /stream endpoint pushes updates to browser via SSE
  - /cmd endpoint receives commands from browser and publishes to mind/cmd
  - /   serves the dashboard HTML
"""

import json
import queue
import threading
import time

import paho.mqtt.client as mqtt
from flask import Flask, Response, jsonify, render_template, request

# ── Config ────────────────────────────────────────────────────
MQTT_HOST   = "localhost"
MQTT_PORT   = 1883              # raw MQTT (paho speaks MQTT, not WebSocket)
FLASK_PORT  = 3000

TOPICS = {
    "data":  "mind/data",
    "alert": "mind/alert",
}

# ── Shared state ──────────────────────────────────────────────
app           = Flask(__name__)
latest_state  = {}               # last known sensor payload

# Each connected SSE client gets a queue; we fan-out messages to all.
client_queues: list[queue.Queue] = []
client_queues_lock = threading.Lock()

def _broadcast(event: str, data: str):
    """Push an SSE event to all connected clients."""
    msg = f"event: {event}\ndata: {data}\n\n"
    with client_queues_lock:
        dead = []
        for q in client_queues:
            try:
                q.put_nowait(msg)
            except queue.Full:
                dead.append(q)
        for q in dead:
            client_queues.remove(q)

# ── MQTT callbacks ────────────────────────────────────────────
def on_connect(client, userdata, flags, rc, properties=None):
    print(f"[MQTT] Connected (rc={rc})")
    for topic in TOPICS.values():
        client.subscribe(topic, qos=0)
    print(f"[MQTT] Subscribed to: {list(TOPICS.values())}")

def on_disconnect(client, userdata, disconnect_flags, rc, properties=None):
    print(f"[MQTT] Disconnected (rc={rc})")

def on_message(client, userdata, msg):
    global latest_state, log_buffer
    topic   = msg.topic
    payload = msg.payload.decode("utf-8", errors="replace")

    # ── mind/data ────────────────────────────────────────────
    if topic == TOPICS["data"]:
        try:
            d = json.loads(payload)
            latest_state.update(d)
            _broadcast("data", payload)
        except json.JSONDecodeError:
            pass

    # ── mind/alert ───────────────────────────────────────────
    elif topic == TOPICS["alert"]:
        try:
            d = json.loads(payload)
            latest_state["emergency"] = d.get("emergency", False)
            _broadcast("alert", payload)
        except json.JSONDecodeError:
            pass

# ── MQTT client setup ─────────────────────────────────────────
def start_mqtt():
    mc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                     client_id="mind-flask-dashboard")
    mc.on_connect    = on_connect
    mc.on_disconnect = on_disconnect
    mc.on_message    = on_message

    # Attempt to connect; retry in background if broker isn't up yet
    connected = False
    while not connected:
        try:
            mc.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
            connected = True
        except Exception as e:
            print(f"[MQTT] Connection failed ({e}), retrying in 3s...")
            time.sleep(3)

    mc.loop_start()   # background thread for keepalive + message dispatch
    return mc

mqtt_client = None   # populated in __main__

# ── Flask routes ──────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("index.html")


@app.route("/stream")
def stream():
    """Server-Sent Events endpoint. Each browser tab gets its own queue."""
    q: queue.Queue = queue.Queue(maxsize=100)

    # Prime with latest state so the page isn't blank on (re)connect
    if latest_state:
        q.put_nowait(f"event: data\ndata: {json.dumps(latest_state)}\n\n")

    with client_queues_lock:
        client_queues.append(q)

    def generate():
        try:
            # Send a heartbeat comment every 15 s to keep the connection alive
            last_heartbeat = time.time()
            while True:
                try:
                    msg = q.get(timeout=15)
                    yield msg
                except queue.Empty:
                    yield ": heartbeat\n\n"
                    last_heartbeat = time.time()
        except GeneratorExit:
            pass
        finally:
            with client_queues_lock:
                if q in client_queues:
                    client_queues.remove(q)

    return Response(
        generate(),
        mimetype="text/event-stream",
        headers={
            "Cache-Control":     "no-cache",
            "X-Accel-Buffering": "no",
        },
    )


@app.route("/cmd", methods=["POST"])
def send_command():
    """Relay a JSON command from the browser to mind/cmd."""
    body = request.get_json(silent=True)
    if not body or "cmd" not in body:
        return jsonify({"error": "Missing 'cmd' field"}), 400

    payload = json.dumps({"cmd": body["cmd"]})
    if mqtt_client:
        mqtt_client.publish("mind/cmd", payload, qos=0)
        return jsonify({"ok": True})
    return jsonify({"error": "MQTT not connected"}), 503


@app.route("/state")
def get_state():
    return jsonify(latest_state)


# ── Entry point ───────────────────────────────────────────────
if __name__ == "__main__":
    mqtt_client = start_mqtt()
    print(f"[SERVER] Dashboard running at http://localhost:{FLASK_PORT}")
    # use_reloader=False so we don't start two MQTT loops in debug mode
    app.run(host="0.0.0.0", port=FLASK_PORT, debug=True, use_reloader=False, threaded=True)
