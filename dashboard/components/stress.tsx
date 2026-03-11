"use client";

import { Card, CardContent } from "@/components/ui/card";
import { ShieldAlert } from "lucide-react";

const STRESS_COLOR: Record<string, string> = {
  Low:      "text-green-400",
  Moderate: "text-yellow-400",
  High:     "text-red-400",
};

export function StressCard({
  stressLevel,
  gsrValue,
}: {
  stressLevel: string;
  gsrValue: number;
}) {
  const color = STRESS_COLOR[stressLevel] ?? "text-muted-foreground";

  return (
    <Card className="gap-0 py-0">
      <CardContent className="px-4 pt-4 pb-4">
        <div className="flex items-center gap-1.5 mb-1">
          <ShieldAlert
            className={`h-3 w-3 ${
              stressLevel === "High" ? "text-red-400" : "text-muted-foreground"
            }`}
          />
          <span className="text-[10px] font-semibold uppercase tracking-widest text-muted-foreground">
            Stress Level
          </span>
        </div>
        <p className={`text-2xl font-bold ${color}`}>{stressLevel}</p>
        <p className="mt-1 font-mono text-xs tabular-nums text-muted-foreground">
          {gsrValue.toFixed(1)} &micro;S
        </p>
      </CardContent>
    </Card>
  );
}
