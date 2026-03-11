"use client";

import { useState } from "react";
import type { LogEntry } from "@/lib/types";
import {
  Card,
  CardHeader,
  CardTitle,
  CardDescription,
  CardContent,
} from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { ScrollText, Trash2 } from "lucide-react";

const LEVEL_STYLE: Record<string, string> = {
  DEBUG: "text-zinc-500",
  INFO: "text-blue-400",
  WARN: "text-yellow-400",
  ERROR: "text-red-400",
};

type FilterLevel = "ALL" | "DEBUG" | "INFO" | "WARN" | "ERROR";

export function LogsPanel({
  logs,
  onClear,
}: {
  logs: LogEntry[];
  onClear: () => void;
}) {
  const [filter, setFilter] = useState<FilterLevel>("ALL");

  const filtered =
    filter === "ALL" ? logs : logs.filter((l) => l.level === filter);

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <ScrollText className="h-4 w-4 text-muted-foreground" />
            <CardTitle>Device Logs</CardTitle>
          </div>
          <div className="flex items-center gap-2">
            <span className="text-xs tabular-nums text-muted-foreground">
              {filtered.length} entries
            </span>
            <Button variant="ghost" size="icon-xs" onClick={onClear}>
              <Trash2 className="h-3.5 w-3.5" />
            </Button>
          </div>
        </div>
        <CardDescription>
          <div className="flex items-center gap-1 pt-1">
            {(["ALL", "DEBUG", "INFO", "WARN", "ERROR"] as FilterLevel[]).map(
              (level) => (
                <Button
                  key={level}
                  variant={filter === level ? "secondary" : "ghost"}
                  size="xs"
                  onClick={() => setFilter(level)}
                  className="h-6 px-2 text-xs"
                >
                  {level}
                </Button>
              )
            )}
          </div>
        </CardDescription>
      </CardHeader>
      <CardContent>
        <ScrollArea className="h-56 rounded-md border border-border bg-background p-3">
          <div className="font-mono text-xs leading-relaxed">
            {filtered.length === 0 ? (
              <p className="text-muted-foreground">Waiting for logs...</p>
            ) : (
              filtered.map((entry, i) => (
                <div key={`${entry.ts}-${i}`} className="flex gap-2">
                  <span className="shrink-0 tabular-nums text-muted-foreground">
                    {(entry.ts / 1000).toFixed(1)}s
                  </span>
                  <span
                    className={`w-11 shrink-0 font-bold ${
                      LEVEL_STYLE[entry.level] ?? "text-muted-foreground"
                    }`}
                  >
                    {entry.level}
                  </span>
                  <span className="shrink-0 text-primary">[{entry.tag}]</span>
                  <span className="break-all text-foreground">{entry.msg}</span>
                </div>
              ))
            )}
          </div>
        </ScrollArea>
      </CardContent>
    </Card>
  );
}
