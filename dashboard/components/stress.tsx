// =============================================================
// Stress Level Card — GSR-based stress classification
// =============================================================
"use client";

import { Card } from "./card";

const STRESS_CONFIG: Record<string, { color: string; bg: string; emoji: string }> = {
  Low: { color: "text-green-400", bg: "bg-green-500/20", emoji: "😊" },
  Moderate: { color: "text-yellow-400", bg: "bg-yellow-500/20", emoji: "😐" },
  High: { color: "text-red-400", bg: "bg-red-500/20", emoji: "😰" },
};

export function StressCard({
  stressLevel,
  gsrValue,
}: {
  stressLevel: string;
  gsrValue: number;
}) {
  const cfg = STRESS_CONFIG[stressLevel] ?? STRESS_CONFIG.Low;

  return (
    <Card
      title="Stress Level"
      icon="😰"
      accent={stressLevel === "High" ? "text-accent-red" : undefined}
    >
      <div className="flex items-center gap-3">
        <span className="text-4xl">{cfg.emoji}</span>
        <div>
          <span
            className={`inline-block rounded-full px-3 py-1 text-sm font-bold ${cfg.color} ${cfg.bg}`}
          >
            {stressLevel}
          </span>
          <p className="mt-1 text-xs text-muted tabular-nums">
            GSR: {gsrValue.toFixed(1)} µS
          </p>
        </div>
      </div>
    </Card>
  );
}
