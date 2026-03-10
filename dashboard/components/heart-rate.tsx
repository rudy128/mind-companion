// =============================================================
// Heart Rate Card — BPM display with pulse animation
// =============================================================
"use client";

import { Card } from "./card";

export function HeartRateCard({
  bpm,
  fingerDetected,
}: {
  bpm: number;
  fingerDetected: boolean;
}) {
  const isAbnormal = bpm > 0 && (bpm > 120 || bpm < 50);
  const bpmColor = isAbnormal
    ? "text-accent-red"
    : bpm > 0
      ? "text-accent-green"
      : "text-muted";

  return (
    <Card title="Heart Rate" icon="🫀" accent={isAbnormal ? "text-accent-red" : undefined}>
      <div className="flex items-baseline gap-2">
        <span
          className={`text-5xl font-bold tabular-nums ${bpmColor} ${
            fingerDetected && bpm > 0 ? "animate-pulse-heart" : ""
          }`}
        >
          {fingerDetected && bpm > 0 ? bpm : "--"}
        </span>
        <span className="text-lg text-muted">BPM</span>
      </div>
      <p className="mt-2 text-sm text-muted">
        {fingerDetected
          ? isAbnormal
            ? "⚠ Abnormal — breathing LED active"
            : "Finger detected"
          : "No finger detected"}
      </p>
    </Card>
  );
}
