"use client";

import {
  Card,
  CardHeader,
  CardTitle,
  CardDescription,
  CardContent,
} from "@/components/ui/card";
import { Progress } from "@/components/ui/progress";
import { Separator } from "@/components/ui/separator";
import { Cpu, Clock, Mic } from "lucide-react";

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
  const heapKB = (heap / 1024).toFixed(1);
  const heapPct = Math.min(100, (heap / (512 * 1024)) * 100);

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center gap-2">
          <Cpu className="h-4 w-4 text-muted-foreground" />
          <CardTitle>Device Info</CardTitle>
        </div>
        <CardDescription>ESP32-S3 system status</CardDescription>
      </CardHeader>
      <CardContent className="space-y-3">
        {/* Heap */}
        <div>
          <div className="mb-1 flex items-center justify-between text-sm">
            <span className="text-muted-foreground">Free Heap</span>
            <span className="font-mono tabular-nums">{heapKB} KB</span>
          </div>
          <Progress value={heapPct} className="h-2" />
        </div>

        <Separator />

        {/* Uptime */}
        <div className="flex items-center justify-between text-sm">
          <span className="flex items-center gap-1.5 text-muted-foreground">
            <Clock className="h-3.5 w-3.5" />
            Uptime
          </span>
          <span className="font-mono tabular-nums">{formatUptime(uptime)}</span>
        </div>

        {/* Voice */}
        <div className="flex items-center justify-between text-sm">
          <span className="flex items-center gap-1.5 text-muted-foreground">
            <Mic className="h-3.5 w-3.5" />
            Last Voice
          </span>
          <span className="max-w-[140px] truncate text-right font-mono text-xs">
            {voiceCommand || "\u2014"}
          </span>
        </div>
      </CardContent>
    </Card>
  );
}
