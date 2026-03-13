// =============================================================
// M.I.N.D. Companion Dashboard — MQTT Hook
// =============================================================
// React hook that manages a single MQTT connection over WebSocket
// to the local Mosquitto broker. Subscribes to all mind/* topics
// and exposes parsed state + a sendCommand function.
//
// Mosquitto must be configured with a WebSocket listener:
//   listener 9001
//   protocol websockets
// =============================================================
"use client";

import { useEffect, useRef, useState, useCallback } from "react";
import mqtt, { MqttClient } from "mqtt";
import type {
  SensorData,
  LogEntry,
  ConnectionState,
  Command,
} from "./types";
import { TOPICS } from "./types";

/** Maximum log entries kept in memory */
const MAX_LOGS = 200;

/** Broker URL — Mosquitto WebSocket listener on your laptop */
const BROKER_URL =
  process.env.NEXT_PUBLIC_MQTT_URL ?? "ws://localhost:9001";

export interface MqttState {
  /** Latest sensor telemetry from the MCU */
  data: SensorData | null;
  /** Rolling log buffer (newest first) */
  logs: LogEntry[];
  /** MQTT connection status */
  connection: ConnectionState;
  /** Latest recorded audio as base64 WAV */
  audioBase64: string | null;
  /** Send a command to the MCU */
  sendCommand: (cmd: Command["cmd"]) => void;
  /** Clear the local log buffer */
  clearLogs: () => void;
  /** Clear the audio buffer */
  clearAudio: () => void;
}

export function useMqtt(): MqttState {
  const clientRef = useRef<MqttClient | null>(null);
  const [data, setData] = useState<SensorData | null>(null);
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [connection, setConnection] = useState<ConnectionState>("disconnected");
  const [audioBase64, setAudioBase64] = useState<string | null>(null);

  // ── Connect on mount, disconnect on unmount ────────────
  useEffect(() => {
    setConnection("connecting");

    const client = mqtt.connect(BROKER_URL, {
      clientId: `mind-dashboard-${Math.random().toString(16).slice(2, 8)}`,
      reconnectPeriod: 3000,    // auto-reconnect every 3s
      connectTimeout: 10_000,
      clean: true,
    });

    clientRef.current = client;

    client.on("connect", () => {
      console.log("[MQTT] Connected to broker:", BROKER_URL);
      setConnection("connected");
      // Subscribe to all mind/* topics
      client.subscribe(
        [TOPICS.DATA, TOPICS.LOGS, TOPICS.ALERT, TOPICS.AUDIO],
        { qos: 0 },
        (err) => {
          if (err) console.error("[MQTT] Subscribe error:", err);
          else console.log("[MQTT] Subscribed to:", TOPICS.DATA, TOPICS.LOGS, TOPICS.ALERT, TOPICS.AUDIO);
        },
      );
    });

    client.on("reconnect", () => { console.log("[MQTT] Reconnecting..."); setConnection("connecting"); });
    client.on("disconnect", () => { console.log("[MQTT] Disconnected"); setConnection("disconnected"); });
    client.on("offline", () => { console.log("[MQTT] Offline"); setConnection("disconnected"); });
    client.on("error", (err) => { console.error("[MQTT] Error:", err); setConnection("error"); });

    client.on("message", (topic: string, payload: Buffer) => {
      const raw = payload.toString();
      console.log(`[MQTT] ${topic} (${raw.length} bytes):`, raw);
      try {
        const msg = JSON.parse(raw);

        switch (topic) {
          case TOPICS.DATA:
            setData(msg as SensorData);
            break;

          case TOPICS.LOGS:
            setLogs((prev) => {
              const next = [msg as LogEntry, ...prev];
              return next.length > MAX_LOGS ? next.slice(0, MAX_LOGS) : next;
            });
            break;

          case TOPICS.ALERT:
            // Alert updates the emergency field in sensor data
            setData((prev) =>
              prev ? { ...prev, emergency: msg.emergency } : prev,
            );
            break;
        }
      } catch (e) {
        // Check if it's the audio topic (raw base64, not JSON)
        if (topic === TOPICS.AUDIO) {
          console.log("[MQTT] Received audio:", raw.length, "bytes base64");
          setAudioBase64(raw);
          return;
        }
        console.error("[MQTT] JSON parse error:", e, "raw:", raw.substring(0, 100));
      }
    });

    return () => {
      client.end(true);
      clientRef.current = null;
    };
  }, []);

  // ── Send command to MCU ────────────────────────────────
  const sendCommand = useCallback((cmd: Command["cmd"]) => {
    const client = clientRef.current;
    if (!client?.connected) return;
    const payload = JSON.stringify({ cmd });
    client.publish(TOPICS.CMD, payload, { qos: 0 });
  }, []);

  // ── Clear logs ─────────────────────────────────────────
  const clearLogs = useCallback(() => {
    setLogs([]);
  }, []);

  // ── Clear audio ────────────────────────────────────────
  const clearAudio = useCallback(() => {
    setAudioBase64(null);
  }, []);

  return { data, logs, connection, audioBase64, sendCommand, clearLogs, clearAudio };
}
