"use client";

import {
  Card,
  CardHeader,
  CardTitle,
  CardDescription,
  CardContent,
} from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Moon } from "lucide-react";

const SLEEP_VARIANT: Record<string, "default" | "secondary" | "outline"> = {
  "Deep Sleep":  "default",
  "Light Sleep": "secondary",
  Restless:      "outline",
  Awake:         "outline",
};

const SLEEP_DESC: Record<string, string> = {
  "Deep Sleep":  "Minimal movement detected",
  "Light Sleep": "Low movement detected",
  Restless:      "Frequent movement detected",
  Awake:         "Active movement",
};

export function SleepCard({ quality }: { quality: string }) {
  const variant = SLEEP_VARIANT[quality] ?? "outline";
  const description = SLEEP_DESC[quality] ?? "Monitoring";

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center gap-2">
          <Moon className="h-4 w-4 text-muted-foreground" />
          <CardTitle>Sleep Quality</CardTitle>
        </div>
        <CardDescription>{description}</CardDescription>
      </CardHeader>
      <CardContent>
        <Badge variant={variant} className="text-sm px-3 py-1">
          {quality}
        </Badge>
      </CardContent>
    </Card>
  );
}
