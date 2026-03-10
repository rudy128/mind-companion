// =============================================================
// Camera Feed — Direct MJPEG stream from ESP32 port 81
// =============================================================
"use client";

import { useState } from "react";
import { Card } from "./card";

/**
 * The camera feed connects directly to the ESP32's HTTP server on port 81.
 * This does NOT go through MQTT — it's a direct browser → ESP32 connection.
 * The ESP32 serves MJPEG frames via esp_http_server (lightweight, ~2 KB RAM).
 */
export function CameraFeed({ espIp }: { espIp: string }) {
  const [open, setOpen] = useState(false);
  const [error, setError] = useState(false);

  const streamUrl = `http://${espIp}:81/stream`;

  return (
    <Card title="Live Camera" icon="📷">
      {!open ? (
        <button
          onClick={() => {
            setOpen(true);
            setError(false);
          }}
          className="w-full rounded-lg border border-card-border bg-zinc-900 py-8 text-sm text-muted hover:border-accent-cyan hover:text-accent-cyan transition-colors cursor-pointer"
        >
          📷 Open Camera Stream
        </button>
      ) : (
        <div className="space-y-2">
          <div className="relative aspect-video w-full overflow-hidden rounded-lg bg-black">
            {error ? (
              <div className="flex h-full items-center justify-center text-sm text-muted">
                <p>Cannot reach camera at {espIp}:81</p>
              </div>
            ) : (
              /* eslint-disable @next/next/no-img-element */
              <img
                src={streamUrl}
                alt="Live camera feed from ESP32"
                className="h-full w-full object-contain"
                onError={() => setError(true)}
              />
            )}
          </div>
          <button
            onClick={() => setOpen(false)}
            className="w-full rounded-lg bg-zinc-800 py-2 text-xs text-muted hover:bg-zinc-700 transition-colors cursor-pointer"
          >
            Close Stream
          </button>
        </div>
      )}
    </Card>
  );
}
