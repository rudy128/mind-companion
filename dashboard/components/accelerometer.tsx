"use client";

import { Card, CardContent } from "@/components/ui/card";
import { Move3d } from "lucide-react";

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
    <Card className="gap-0 py-0">
      <CardContent className="px-4 pt-4 pb-4">
        <div className="flex items-center gap-1.5 mb-3">
          <Move3d className="h-3 w-3 text-muted-foreground" />
          <span className="text-[10px] font-semibold uppercase tracking-widest text-muted-foreground">
            Motion / IMU
          </span>
        </div>
        <div className="grid grid-cols-2 gap-x-6 gap-y-2">
          {([
            { label: "X", value: ax },
            { label: "Y", value: ay },
            { label: "Z", value: az },
            { label: "Temp", value: temp, unit: "°C" },
          ] as { label: string; value: number; unit?: string }[]).map(({ label, value, unit }) => (
            <div key={label}>
              <p className="text-[10px] text-muted-foreground">{label}</p>
              <p className="font-mono text-sm tabular-nums">
                {unit ? `${value.toFixed(1)}${unit}` : `${value.toFixed(3)}g`}
              </p>
            </div>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}
