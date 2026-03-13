// =============================================================
// M.I.N.D. Companion Dashboard — Shared Types
// =============================================================
// These types mirror the JSON payloads published by the ESP32
// MQTT client in mqtt_client.cpp.
// =============================================================

/** Sensor telemetry — published on `mind/data` at 1 Hz */
export interface SensorData {
  bpm: number;
  finger: boolean;
  gsr: number;
  stress: "Low" | "Moderate" | "High" | string;
  sleep: "Deep Sleep" | "Light Sleep" | "Restless" | "Awake" | string;
  emergency: boolean;
  ax: number;
  ay: number;
  az: number;
  temp: number;
  voice: string;
  breathing: boolean;
  camOpen: boolean;
  ip: string;
  heap: number;
  uptime: number;
}

/** Log entry — published on `mind/logs` */
export interface LogEntry {
  ts: number;
  level: "DEBUG" | "INFO" | "WARN" | "ERROR";
  tag: string;
  msg: string;
}

/** Emergency alert — published on `mind/alert` (retained) */
export interface AlertMessage {
  emergency: boolean;
  ts: number;
}

/** Command sent from dashboard to MCU on `mind/cmd` */
export interface Command {
  cmd:
    | "breathe"
    | "alarm_on"
    | "alarm_off"
    | "vibrate"
    | "clear_emergency";
}

/** MQTT connection state for UI display */
export type ConnectionState = "connected" | "connecting" | "disconnected" | "error";

/** MQTT topic names — must match config.h on the MCU */
export const TOPICS = {
  DATA: "mind/data",
  LOGS: "mind/logs",
  ALERT: "mind/alert",
  CMD: "mind/cmd",
  AUDIO: "mind/audio",
  AI_RESPONSE: "mind/ai_response",
} as const;
