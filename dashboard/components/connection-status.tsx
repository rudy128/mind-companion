// =============================================================
// Connection Status — MQTT broker connection indicator
// =============================================================
"use client";

import type { ConnectionState } from "@/lib/types";

const STATUS_CONFIG: Record<
  ConnectionState,
  { color: string; label: string; dot: string }
> = {
  connected: {
    color: "text-accent-green",
    label: "Connected",
    dot: "bg-green-500",
  },
  connecting: {
    color: "text-accent-yellow",
    label: "Connecting…",
    dot: "bg-yellow-500 animate-pulse",
  },
  disconnected: {
    color: "text-muted",
    label: "Disconnected",
    dot: "bg-zinc-500",
  },
  error: {
    color: "text-accent-red",
    label: "Error",
    dot: "bg-red-500",
  },
};

export function ConnectionStatus({ state }: { state: ConnectionState }) {
  const cfg = STATUS_CONFIG[state];
  return (
    <div className="flex items-center gap-2">
      <span className={`inline-block h-2.5 w-2.5 rounded-full ${cfg.dot}`} />
      <span className={`text-sm font-medium ${cfg.color}`}>
        MQTT: {cfg.label}
      </span>
    </div>
  );
}
