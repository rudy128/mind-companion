"use client";

import type { Command } from "@/lib/types";
import {
  Card,
  CardHeader,
  CardTitle,
  CardDescription,
  CardContent,
} from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import {
  Tooltip,
  TooltipContent,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import {
  Lightbulb,
  BellRing,
  BellOff,
  Vibrate,
  Sliders,
} from "lucide-react";

interface ControlDef {
  label: string;
  cmd: Command["cmd"];
  Icon: typeof Lightbulb;
  variant: "default" | "destructive" | "secondary" | "outline";
  tooltip: string;
}

const CONTROLS: ControlDef[] = [
  {
    label: "Breathe",
    cmd: "breathe",
    Icon: Lightbulb,
    variant: "default",
    tooltip: "Activate LED breathing pattern",
  },
  {
    label: "Alarm ON",
    cmd: "alarm_on",
    Icon: BellRing,
    variant: "destructive",
    tooltip: "Start emergency alarm",
  },
  {
    label: "Alarm OFF",
    cmd: "alarm_off",
    Icon: BellOff,
    variant: "secondary",
    tooltip: "Stop emergency alarm",
  },
  {
    label: "Vibrate",
    cmd: "vibrate",
    Icon: Vibrate,
    variant: "outline",
    tooltip: "Trigger vibration motor",
  },
];

export function ControlsPanel({
  sendCommand,
  breathingActive,
}: {
  sendCommand: (cmd: Command["cmd"]) => void;
  breathingActive: boolean;
}) {
  return (
    <Card>
      <CardHeader>
        <div className="flex items-center gap-2">
          <Sliders className="h-4 w-4 text-muted-foreground" />
          <CardTitle>Device Controls</CardTitle>
        </div>
        <CardDescription>Send commands via MQTT</CardDescription>
      </CardHeader>
      <CardContent>
        <div className="grid grid-cols-2 gap-2">
          {CONTROLS.map((ctrl) => (
            <Tooltip key={ctrl.cmd}>
              <TooltipTrigger asChild>
                <Button
                  variant={ctrl.variant}
                  size="sm"
                  className="relative w-full justify-start gap-2"
                  onClick={() => sendCommand(ctrl.cmd)}
                >
                  <ctrl.Icon className="h-4 w-4" />
                  {ctrl.label}
                  {ctrl.cmd === "breathe" && breathingActive && (
                    <span className="absolute top-1 right-1 h-2 w-2 rounded-full bg-primary animate-breathe" />
                  )}
                </Button>
              </TooltipTrigger>
              <TooltipContent>
                <p>{ctrl.tooltip}</p>
              </TooltipContent>
            </Tooltip>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}
