"use client";

import { useState, useEffect } from "react";
import { Card, CardContent } from "@/components/ui/card";
import {
  AreaChart,
  Area,
  ResponsiveContainer,
  Tooltip,
  YAxis,
} from "recharts";
import { Heart } from "lucide-react";

const MAX_HISTORY = 50;

export function HeartRateCard({
  bpm,
  fingerDetected,
}: {
  bpm: number;
  fingerDetected: boolean;
}) {
  const [history, setHistory] = useState<number[]>([]);

  // FILO stack with consecutive-duplicate deduplication
  useEffect(() => {
    const val = fingerDetected && bpm > 0 ? bpm : 0;
    setHistory((prev) => {
      if (prev.length > 0 && prev[prev.length - 1] === val) return prev;
      const next = [...prev, val];
      return next.length > MAX_HISTORY ? next.slice(1) : next;
    });
  }, [bpm, fingerDetected]);

  const chartData = history.map((v, i) => ({
    t: i,
    bpm: v > 0 ? v : null,
  }));

  const isAbnormal = bpm > 0 && (bpm > 120 || bpm < 50);
  const displayBpm = fingerDetected && bpm > 0;
  const lineColor = isAbnormal ? "#f87171" : "#4ade80";

  return (
    <Card className="gap-0 overflow-hidden py-0">
      <CardContent className="px-4 pt-4 pb-2">
        <div className="flex items-center gap-1.5 mb-1">
          <Heart
            className={`h-3 w-3 ${
              isAbnormal ? "text-destructive" : "text-muted-foreground"
            }`}
          />
          <span className="text-[10px] font-semibold uppercase tracking-widest text-muted-foreground">
            Heart Rate
          </span>
        </div>
        <div className="flex items-baseline gap-1">
          <span
            className={`text-3xl font-bold tabular-nums ${
              isAbnormal
                ? "text-destructive"
                : displayBpm
                  ? "text-green-400"
                  : "text-muted-foreground"
            }`}
          >
            {displayBpm ? bpm : "--"}
          </span>
          <span className="text-xs text-muted-foreground">bpm</span>
        </div>
        <p className="mt-0.5 text-xs text-muted-foreground">
          {fingerDetected
            ? isAbnormal
              ? "Abnormal range"
              : "Normal"
            : "No finger"}
        </p>
      </CardContent>

      {/* HR graph — always rendered, fills when data arrives */}
      <div className="h-[72px] w-full">
        <ResponsiveContainer width="100%" height="100%">
          <AreaChart
            data={chartData}
            margin={{ top: 2, right: 0, left: 0, bottom: 0 }}
          >
            <defs>
              <linearGradient id="hrFill" x1="0" y1="0" x2="0" y2="1">
                <stop offset="5%" stopColor={lineColor} stopOpacity={0.25} />
                <stop offset="95%" stopColor={lineColor} stopOpacity={0} />
              </linearGradient>
            </defs>
            <YAxis domain={["auto", "auto"]} hide />
            <Area
              type="monotone"
              dataKey="bpm"
              stroke={lineColor}
              strokeWidth={1.5}
              fill="url(#hrFill)"
              dot={false}
              connectNulls={false}
              isAnimationActive={false}
            />
            <Tooltip
              contentStyle={{
                background: "#18181b",
                border: "1px solid #27272a",
                borderRadius: "4px",
                fontSize: "11px",
                padding: "2px 8px",
              }}
              itemStyle={{ color: lineColor }}
              formatter={(v: number) => [`${v} bpm`]}
              labelFormatter={() => ""}
              cursor={{ stroke: lineColor, strokeWidth: 1, strokeDasharray: "3 3" }}
            />
          </AreaChart>
        </ResponsiveContainer>
      </div>
    </Card>
  );
}
