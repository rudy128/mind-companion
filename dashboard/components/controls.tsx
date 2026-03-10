// =============================================================
// Controls Panel — send commands to MCU via MQTT
// =============================================================
"use client";

import type { Command } from "@/lib/types";
import { Card } from "./card";

interface ControlButton {
  label: string;
  cmd: Command["cmd"];
  icon: string;
  color: string;
  hoverColor: string;
}

const BUTTONS: ControlButton[] = [
  {
    label: "Breathe",
    cmd: "breathe",
    icon: "💡",
    color: "bg-cyan-600/20 text-cyan-400 border-cyan-500/30",
    hoverColor: "hover:bg-cyan-600/40",
  },
  {
    label: "Alarm ON",
    cmd: "alarm_on",
    icon: "🔔",
    color: "bg-red-600/20 text-red-400 border-red-500/30",
    hoverColor: "hover:bg-red-600/40",
  },
  {
    label: "Alarm OFF",
    cmd: "alarm_off",
    icon: "🔕",
    color: "bg-zinc-600/20 text-zinc-400 border-zinc-500/30",
    hoverColor: "hover:bg-zinc-600/40",
  },
  {
    label: "Vibrate",
    cmd: "vibrate",
    icon: "📳",
    color: "bg-purple-600/20 text-purple-400 border-purple-500/30",
    hoverColor: "hover:bg-purple-600/40",
  },
];

export function ControlsPanel({
  sendCommand,
  breathingActive,
}: {
  sendCommand: (cmd: Command["cmd"]) => void;
  breathingActive: boolean;
}) {
  return (
    <Card title="Device Controls" icon="🎮">
      <div className="grid grid-cols-2 gap-2">
        {BUTTONS.map((btn) => (
          <button
            key={btn.cmd}
            onClick={() => sendCommand(btn.cmd)}
            className={`relative rounded-lg border px-3 py-3 text-sm font-medium transition-colors cursor-pointer ${btn.color} ${btn.hoverColor}`}
          >
            <span className="mr-1.5">{btn.icon}</span>
            {btn.label}
            {btn.cmd === "breathe" && breathingActive && (
              <span className="absolute top-1 right-1 h-2 w-2 rounded-full bg-cyan-400 animate-breathe" />
            )}
          </button>
        ))}
      </div>
      <p className="mt-3 text-xs text-muted">
        Commands are sent instantly via MQTT to the device.
      </p>
    </Card>
  );
}
