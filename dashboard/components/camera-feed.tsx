"use client";

import { useState } from "react";
import {
  Card,
  CardHeader,
  CardTitle,
  CardDescription,
  CardContent,
} from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Camera, CameraOff } from "lucide-react";

export function CameraFeed({ espIp }: { espIp: string }) {
  const [open, setOpen] = useState(false);
  const [error, setError] = useState(false);

  const streamUrl = `http://${espIp}:81/stream`;

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center gap-2">
          <Camera className="h-4 w-4 text-muted-foreground" />
          <CardTitle>Live Camera</CardTitle>
        </div>
        <CardDescription>MJPEG stream from ESP32</CardDescription>
      </CardHeader>
      <CardContent>
        {!open ? (
          <Button
            variant="outline"
            className="w-full"
            onClick={() => {
              setOpen(true);
              setError(false);
            }}
          >
            <Camera className="mr-2 h-4 w-4" />
            Open Camera Stream
          </Button>
        ) : (
          <div className="space-y-2">
            <div className="relative aspect-video w-full overflow-hidden rounded-md border border-border bg-black">
              {error ? (
                <div className="flex h-full items-center justify-center">
                  <div className="text-center text-sm text-muted-foreground">
                    <CameraOff className="mx-auto mb-2 h-6 w-6" />
                    <p>Cannot reach camera at {espIp}:81</p>
                  </div>
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
            <Button
              variant="secondary"
              size="sm"
              className="w-full"
              onClick={() => setOpen(false)}
            >
              Close Stream
            </Button>
          </div>
        )}
      </CardContent>
    </Card>
  );
}
