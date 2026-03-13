// =============================================================
// Audio Debug Component — Play recorded mic audio from ESP32
// =============================================================
"use client";

import { useEffect, useRef, useState } from "react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Mic, Play, Pause, Trash2, Download } from "lucide-react";

interface AudioDebugProps {
  audioBase64: string | null;
  onClear: () => void;
}

export function AudioDebug({ audioBase64, onClear }: AudioDebugProps) {
  const audioRef = useRef<HTMLAudioElement>(null);
  const [isPlaying, setIsPlaying] = useState(false);
  const [audioUrl, setAudioUrl] = useState<string | null>(null);

  // Convert base64 to blob URL when audio arrives
  useEffect(() => {
    if (!audioBase64) {
      setAudioUrl(null);
      return;
    }

    try {
      // Decode base64 to binary
      const binaryString = atob(audioBase64);
      const bytes = new Uint8Array(binaryString.length);
      for (let i = 0; i < binaryString.length; i++) {
        bytes[i] = binaryString.charCodeAt(i);
      }

      // Create blob and URL
      const blob = new Blob([bytes], { type: "audio/wav" });
      const url = URL.createObjectURL(blob);
      setAudioUrl(url);

      // Cleanup previous URL
      return () => URL.revokeObjectURL(url);
    } catch (e) {
      console.error("Failed to decode audio:", e);
    }
  }, [audioBase64]);

  const handlePlayPause = () => {
    if (!audioRef.current || !audioUrl) return;

    if (isPlaying) {
      audioRef.current.pause();
    } else {
      audioRef.current.play();
    }
  };

  const handleDownload = () => {
    if (!audioUrl) return;
    const a = document.createElement("a");
    a.href = audioUrl;
    a.download = `mic-recording-${Date.now()}.wav`;
    a.click();
  };

  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="flex items-center gap-2 text-lg">
          <Mic className="h-5 w-5" />
          Mic Debug
        </CardTitle>
        <CardDescription>
          Listen to what the ESP32 microphone recorded
        </CardDescription>
      </CardHeader>
      <CardContent>
        {audioUrl ? (
          <div className="space-y-3">
            <audio
              ref={audioRef}
              src={audioUrl}
              onPlay={() => setIsPlaying(true)}
              onPause={() => setIsPlaying(false)}
              onEnded={() => setIsPlaying(false)}
              className="w-full"
              controls
            />
            <div className="flex gap-2">
              <Button
                variant="outline"
                size="sm"
                onClick={handlePlayPause}
              >
                {isPlaying ? (
                  <><Pause className="h-4 w-4 mr-1" /> Pause</>
                ) : (
                  <><Play className="h-4 w-4 mr-1" /> Play</>
                )}
              </Button>
              <Button
                variant="outline"
                size="sm"
                onClick={handleDownload}
              >
                <Download className="h-4 w-4 mr-1" /> Download
              </Button>
              <Button
                variant="destructive"
                size="sm"
                onClick={onClear}
              >
                <Trash2 className="h-4 w-4 mr-1" /> Clear
              </Button>
            </div>
            <p className="text-xs text-muted-foreground">
              {Math.round((audioBase64?.length || 0) / 1024)} KB base64 
              → ~{Math.round((audioBase64?.length || 0) * 0.75 / 1024)} KB WAV
            </p>
          </div>
        ) : (
          <div className="text-center py-6 text-muted-foreground">
            <Mic className="h-8 w-8 mx-auto mb-2 opacity-50" />
            <p>No audio recorded yet</p>
            <p className="text-xs mt-1">Double-press the button on ESP32 to record</p>
          </div>
        )}
      </CardContent>
    </Card>
  );
}
