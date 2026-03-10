// =============================================================
// Logs Panel — live streaming logs from MCU via MQTT
// =============================================================
"use client";

import { useState } from "react";
import type { LogEntry } from "@/lib/types";
import { Card } from "./card";

const LEVEL_STYLE: Record<string, string> = {
  DEBUG: "text-zinc-500",
  INFO: "text-blue-400",
  WARN: "text-yellow-400",
  ERROR: "text-red-400",
};

type FilterLevel = "ALL" | "DEBUG" | "INFO" | "WARN" | "ERROR";

export function LogsPanel({
  logs,
  onClear,
}: {
  logs: LogEntry[];
  onClear: () => void;
}) {
  const [filter, setFilter] = useState<FilterLevel>("ALL");

  const filtered =
    filter === "ALL" ? logs : logs.filter((l) => l.level === filter);

  return (
    <Card title="Device Logs" icon="📋" className="col-span-full">
      {/* Toolbar */}
      <div className="mb-3 flex items-center gap-2 flex-wrap">
        {(["ALL", "DEBUG", "INFO", "WARN", "ERROR"] as FilterLevel[]).map(
          (level) => (
            <button
              key={level}
              onClick={() => setFilter(level)}
              className={`rounded-md px-2.5 py-1 text-xs font-medium transition-colors cursor-pointer ${
                filter === level
                  ? "bg-zinc-700 text-foreground"
                  : "text-muted hover:text-foreground"
              }`}
            >
              {level}
            </button>
          ),
        )}
        <div className="flex-1" />
        <span className="text-xs text-muted tabular-nums">
          {filtered.length} entries
        </span>
        <button
          onClick={onClear}
          className="rounded-md px-2.5 py-1 text-xs text-muted hover:text-foreground transition-colors cursor-pointer"
        >
          Clear
        </button>
      </div>

      {/* Log lines */}
      <div className="logs-scroll h-56 overflow-y-auto rounded-lg bg-zinc-950 p-3 font-mono text-xs leading-relaxed">
        {filtered.length === 0 ? (
          <p className="text-muted">Waiting for logs…</p>
        ) : (
          filtered.map((entry, i) => (
            <div key={`${entry.ts}-${i}`} className="flex gap-2">
              <span className="text-zinc-600 tabular-nums shrink-0">
                {(entry.ts / 1000).toFixed(1)}s
              </span>
              <span
                className={`w-11 shrink-0 font-bold ${
                  LEVEL_STYLE[entry.level] ?? "text-muted"
                }`}
              >
                {entry.level}
              </span>
              <span className="text-cyan-600 shrink-0">[{entry.tag}]</span>
              <span className="text-zinc-300 break-all">{entry.msg}</span>
            </div>
          ))
        )}
      </div>
    </Card>
  );
}
