"use client";

import type { ConnectionState } from "@/lib/types";
import { Badge } from "@/components/ui/badge";
import { Wifi, WifiOff, Loader2, CircleAlert } from "lucide-react";

const STATUS_MAP: Record<
  ConnectionState,
  { label: string; variant: "default" | "secondary" | "destructive" | "outline"; Icon: typeof Wifi }
> = {
  connected:    { label: "Connected",     variant: "default",     Icon: Wifi },
  connecting:   { label: "Connecting",    variant: "secondary",   Icon: Loader2 },
  disconnected: { label: "Disconnected",  variant: "outline",     Icon: WifiOff },
  error:        { label: "Error",         variant: "destructive", Icon: CircleAlert },
};

export function ConnectionStatus({ state }: { state: ConnectionState }) {
  const { label, variant, Icon } = STATUS_MAP[state];
  return (
    <Badge variant={variant} className="gap-1.5 px-2.5 py-1">
      <Icon className={`h-3 w-3 ${state === "connecting" ? "animate-spin" : ""}`} />
      <span className="text-xs">MQTT: {label}</span>
    </Badge>
  );
}
