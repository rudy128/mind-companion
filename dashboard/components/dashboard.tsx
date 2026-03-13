"use client";

import { useState, useEffect } from "react";
import { useMqtt } from "@/lib/use-mqtt";
import { ConnectionStatus } from "@/components/connection-status";
import { EmergencyBanner } from "@/components/emergency-banner";
import { HeartRateCard } from "@/components/heart-rate";
import { StressCard } from "@/components/stress";
import { SleepCard } from "@/components/sleep";
import { AccelerometerCard } from "@/components/accelerometer";
import { CameraFeed } from "@/components/camera-feed";
import { ControlsPanel } from "@/components/controls";
import { LogsPanel } from "@/components/logs";
import { DeviceInfoCard } from "@/components/device-info";
import { AudioDebug } from "@/components/audio-debug";
import { Input } from "@/components/ui/input";
import { Separator } from "@/components/ui/separator";
import { Activity, Loader2, WifiOff } from "lucide-react";

const DEFAULT_ESP_IP = "172.20.10.8";

export function Dashboard() {
  const { data, logs, connection, audioBase64, aiResponse, sendCommand, clearLogs, clearAudio } = useMqtt();
  const [espIp, setEspIp] = useState(DEFAULT_ESP_IP);

  useEffect(() => {
    if (data?.ip && data.ip !== "0.0.0.0") {
      setEspIp(data.ip);
    }
  }, [data?.ip]);

  return (
    <div className="min-h-screen bg-background">
      {/* Header */}
      <header className="sticky top-0 z-50 border-b border-border bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
        <div className="mx-auto flex h-14 max-w-screen-2xl items-center justify-between px-4 md:px-6 lg:px-8">
          <div className="flex items-center gap-3">
            <Activity className="h-5 w-5 text-primary" />
            <div>
              <h1 className="text-base font-semibold tracking-tight">
                M.I.N.D. Companion
              </h1>
              <p className="hidden text-xs text-muted-foreground sm:block">
                Real-time wellness monitoring
              </p>
            </div>
          </div>
          <div className="flex items-center gap-4">
            <div className="flex items-center gap-2">
              <label htmlFor="esp-ip" className="text-xs text-muted-foreground">
                Device IP
              </label>
              <Input
                id="esp-ip"
                type="text"
                value={espIp}
                onChange={(e) => setEspIp(e.target.value)}
                className="h-7 w-36 font-mono text-xs"
                placeholder="192.168.x.x"
              />
            </div>
            <Separator orientation="vertical" className="h-6" />
            <ConnectionStatus state={connection} />
          </div>
        </div>
      </header>

      <main className="mx-auto max-w-screen-2xl p-4 md:p-6 lg:p-8">
        {/* Emergency Banner */}
        {data?.emergency && (
          <div className="mb-6">
            <EmergencyBanner
              active={true}
              onClear={() => sendCommand("clear_emergency")}
            />
          </div>
        )}

        {/* Waiting: connected but no data */}
        {!data && connection === "connected" && (
          <div className="mb-6 flex flex-col items-center justify-center rounded-lg border border-border bg-card p-12 text-center">
            <Loader2 className="mb-3 h-8 w-8 animate-spin text-muted-foreground" />
            <p className="text-sm font-medium">Waiting for sensor data</p>
            <p className="mt-1 text-xs text-muted-foreground">
              Data will appear when the ESP32 starts publishing.
            </p>
          </div>
        )}

        {/* Waiting: not connected */}
        {!data && connection !== "connected" && (
          <div className="mb-6 flex flex-col items-center justify-center rounded-lg border border-border bg-card p-12 text-center">
            <WifiOff className="mb-3 h-8 w-8 text-muted-foreground" />
            <p className="text-sm font-medium">Connecting to MQTT broker</p>
            <p className="mt-1 text-xs text-muted-foreground">
              Ensure Mosquitto is running on{" "}
              <code className="rounded bg-muted px-1 py-0.5 font-mono text-xs">
                localhost:9001
              </code>
            </p>
          </div>
        )}

        {/* Sensor Grid */}
        {data && (
          <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
            <HeartRateCard bpm={data.bpm} fingerDetected={data.finger} />
            <StressCard stressLevel={data.stress} gsrValue={data.gsr} />
            <SleepCard quality={data.sleep} />
            <AccelerometerCard
              ax={Number(data.ax)}
              ay={Number(data.ay)}
              az={Number(data.az)}
              temp={Number(data.temp)}
            />
            <CameraFeed espIp={espIp} />
            <ControlsPanel
              sendCommand={sendCommand}
              breathingActive={data.breathing}
            />
            <DeviceInfoCard
              heap={data.heap}
              uptime={data.uptime}
              voiceCommand={data.voice}
            />
            <AudioDebug audioBase64={audioBase64} aiResponse={aiResponse} onClear={clearAudio} />
          </div>
        )}

        {/* Logs */}
        <div className="mt-6">
          <LogsPanel logs={logs} onClear={clearLogs} />
        </div>
      </main>

      <footer className="border-t border-border py-4 text-center text-xs text-muted-foreground">
        M.I.N.D. Companion &middot; ESP32-S3 &rarr; MQTT &rarr; Dashboard
      </footer>
    </div>
  );
}
