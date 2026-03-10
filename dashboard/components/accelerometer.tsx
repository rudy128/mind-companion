// =============================================================
// Accelerometer Card — X/Y/Z motion display + temperature
// =============================================================
"use client";

import { Card } from "./card";

function AxisBar({ label, value, color }: { label: string; value: number; color: string }) {
  // Map -2g..+2g to 0..100% bar width
  const pct = Math.min(100, Math.max(0, ((value + 2) / 4) * 100));
  return (
    <div className="flex items-center gap-2">
      <span className="w-5 text-xs font-bold text-muted">{label}</span>
      <div className="flex-1 h-2 rounded-full bg-zinc-800 overflow-hidden">
        <div
          className={`h-full rounded-full ${color} transition-all duration-300`}
          style={{ width: `${pct}%` }}
        />
      </div>
      <span className="w-14 text-right text-xs tabular-nums text-muted">
        {value.toFixed(2)}g
      </span>
    </div>
  );
}

export function AccelerometerCard({
  ax,
  ay,
  az,
  temp,
}: {
  ax: number;
  ay: number;
  az: number;
  temp: number;
}) {
  return (
    <Card title="Motion / IMU" icon="📐">
      <div className="space-y-2">
        <AxisBar label="X" value={ax} color="bg-red-500" />
        <AxisBar label="Y" value={ay} color="bg-green-500" />
        <AxisBar label="Z" value={az} color="bg-blue-500" />
      </div>
      <p className="mt-3 text-xs text-muted">
        🌡 Temp: <span className="text-foreground tabular-nums">{temp.toFixed(1)}°C</span>
      </p>
    </Card>
  );
}
