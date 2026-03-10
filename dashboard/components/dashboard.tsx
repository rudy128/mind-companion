// =============================================================
// Dashboard — Main client component
// =============================================================
// Connects to MQTT broker via WebSocket, subscribes to all
// mind/* topics, and renders the full dashboard with real-time
// updates. No HTTP polling — pure push via MQTT.
// =============================================================
"use client";

import { useState, useEffect } from "react";
import { useMqtt } from "@/lib/use-mqtt";
import { ConnectionStatus } from "@/components/connection-status";
import { EmergencyBanner } from "@/components/emergency-banner";
import { HeartRateCard } from "@/components/heart-rate";
import { StressCard } from "@/components/stress";
import { SleepCard } from "@/components/sleep";
import { AccelerometerCard } from "@/components/accelerometer";
import { CameraFeed } from "@/components/camera-feed";
import { ControlsPanel } from "@/components/controls";
import { LogsPanel } from "@/components/logs";
import { DeviceInfoCard } from "@/components/device-info";

/**
 * Default ESP32 IP — user can change via the input field.
 * Camera stream connects directly to this IP on port 81.
 */
const DEFAULT_ESP_IP = "172.20.10.8";

export function Dashboard() {
  const { data, logs, connection, sendCommand, clearLogs } = useMqtt();
  const [espIp, setEspIp] = useState(DEFAULT_ESP_IP);

  // Auto-update ESP32 IP from MQTT data when available
  useEffect(() => {
    if (data?.ip && data.ip !== "0.0.0.0") {
      setEspIp(data.ip);
    }
  }, [data?.ip]);

  return (
    <div className="min-h-screen bg-background p-4 md:p-6 lg:p-8">
      {/* ── Header ─────────────────────────────────────── */}
      <header className="mb-6 flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
        <div>
          <h1 className="text-2xl font-bold tracking-tight">
            🧠 M.I.N.D. Companion
          </h1>
          <p className="text-sm text-muted">
            Real-time mental wellness monitoring
          </p>
        </div>
        <div className="flex items-center gap-4">
          {/* ESP32 IP input for camera */}
          <div className="flex items-center gap-2">
            <label htmlFor="esp-ip" className="text-xs text-muted">
              ESP32 IP:
            </label>
            <input
              id="esp-ip"
              type="text"
              value={espIp}
              onChange={(e) => setEspIp(e.target.value)}
              className="w-36 rounded-md border border-card-border bg-card px-2 py-1 text-xs font-mono text-foreground focus:border-accent-cyan focus:outline-none"
              placeholder="192.168.x.x"
            />
          </div>
          <ConnectionStatus state={connection} />
        </div>
      </header>

      {/* ── Emergency Banner ───────────────────────────── */}
      {data?.emergency && (
        <div className="mb-6">
          <EmergencyBanner
            active={true}
            onClear={() => sendCommand("clear_emergency")}
          />
        </div>
      )}

      {/* ── Waiting state ──────────────────────────────── */}
      {!data && connection === "connected" && (
        <div className="mb-6 rounded-xl border border-card-border bg-card p-8 text-center">
          <p className="text-lg text-muted">⏳ Waiting for sensor data from device…</p>
          <p className="mt-1 text-sm text-zinc-600">
            Data will appear automatically when the ESP32 starts publishing.
          </p>
        </div>
      )}

      {!data && connection !== "connected" && (
        <div className="mb-6 rounded-xl border border-card-border bg-card p-8 text-center">
          <p className="text-lg text-muted">🔌 Connecting to MQTT broker…</p>
          <p className="mt-1 text-sm text-zinc-600">
            Make sure Mosquitto is running on <code className="font-mono text-accent-cyan">localhost:9001</code> (WebSocket)
          </p>
        </div>
      )}

      {/* ── Sensor Grid ────────────────────────────────── */}
      {data && (
        <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
          {/* Row 1: Vital signs */}
          <HeartRateCard bpm={data.bpm} fingerDetected={data.finger} />
          <StressCard stressLevel={data.stress} gsrValue={data.gsr} />
          <SleepCard quality={data.sleep} />
          <AccelerometerCard
            ax={Number(data.ax)}
            ay={Number(data.ay)}
            az={Number(data.az)}
            temp={Number(data.temp)}
          />

          {/* Row 2: Camera + Controls + Device Info */}
          <CameraFeed espIp={espIp} />
          <ControlsPanel
            sendCommand={sendCommand}
            breathingActive={data.breathing}
          />
          <DeviceInfoCard
            heap={data.heap}
            uptime={data.uptime}
            voiceCommand={data.voice}
          />
        </div>
      )}

      {/* ── Logs Panel ─────────────────────────────────── */}
      <div className="mt-6">
        <LogsPanel logs={logs} onClear={clearLogs} />
      </div>

      {/* ── Footer ─────────────────────────────────────── */}
      <footer className="mt-8 text-center text-xs text-zinc-600">
        M.I.N.D. Companion • ESP32-S3 → MQTT → Dashboard • No HTTP polling
      </footer>
    </div>
  );
}
