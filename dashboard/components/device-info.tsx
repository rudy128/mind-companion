// =============================================================
// Device Info Card — heap, uptime, voice command, connection
// =============================================================
"use client";

import { Card } from "./card";

function formatUptime(seconds: number): string {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

export function DeviceInfoCard({
  heap,
  uptime,
  voiceCommand,
}: {
  heap: number;
  uptime: number;
  voiceCommand: string;
}) {
  // Heap in KB for readability
  const heapKB = (heap / 1024).toFixed(1);
  const heapPct = Math.min(100, (heap / (512 * 1024)) * 100); // ~512KB total SRAM
  const heapColor =
    heapPct > 40 ? "text-green-400" : heapPct > 20 ? "text-yellow-400" : "text-red-400";

  return (
    <Card title="Device Info" icon="🧠">
      <div className="space-y-2 text-sm">
        {/* Heap */}
        <div className="flex items-center justify-between">
          <span className="text-muted">Free Heap</span>
          <span className={`font-mono tabular-nums ${heapColor}`}>
            {heapKB} KB
          </span>
        </div>
        <div className="h-1.5 rounded-full bg-zinc-800 overflow-hidden">
          <div
            className={`h-full rounded-full transition-all duration-500 ${
              heapPct > 40 ? "bg-green-500" : heapPct > 20 ? "bg-yellow-500" : "bg-red-500"
            }`}
            style={{ width: `${heapPct}%` }}
          />
        </div>

        {/* Uptime */}
        <div className="flex items-center justify-between">
          <span className="text-muted">Uptime</span>
          <span className="font-mono tabular-nums text-foreground">
            {formatUptime(uptime)}
          </span>
        </div>

        {/* Last voice command */}
        <div className="flex items-center justify-between">
          <span className="text-muted">Last Voice</span>
          <span className="max-w-[140px] truncate text-right font-mono text-xs text-foreground">
            {voiceCommand || "—"}
          </span>
        </div>
      </div>
    </Card>
  );
}
