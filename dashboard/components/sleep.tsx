// =============================================================
// Sleep Quality Card — movement-based sleep classification
// =============================================================
"use client";

import { Card } from "./card";

const SLEEP_CONFIG: Record<string, { color: string; bg: string; emoji: string }> = {
  "Deep Sleep": { color: "text-purple-400", bg: "bg-purple-500/20", emoji: "😴" },
  "Light Sleep": { color: "text-blue-400", bg: "bg-blue-500/20", emoji: "💤" },
  Restless: { color: "text-yellow-400", bg: "bg-yellow-500/20", emoji: "😣" },
  Awake: { color: "text-cyan-400", bg: "bg-cyan-500/20", emoji: "👀" },
};

export function SleepCard({ quality }: { quality: string }) {
  const cfg = SLEEP_CONFIG[quality] ?? SLEEP_CONFIG.Awake;

  return (
    <Card title="Sleep Quality" icon="😴">
      <div className="flex items-center gap-3">
        <span className="text-4xl">{cfg.emoji}</span>
        <span
          className={`inline-block rounded-full px-3 py-1 text-sm font-bold ${cfg.color} ${cfg.bg}`}
        >
          {quality}
        </span>
      </div>
    </Card>
  );
}
