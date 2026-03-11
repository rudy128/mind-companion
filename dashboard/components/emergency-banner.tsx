"use client";

import { Alert, AlertTitle, AlertDescription } from "@/components/ui/alert";
import { Button } from "@/components/ui/button";
import { AlertTriangle } from "lucide-react";

export function EmergencyBanner({
  active,
  onClear,
}: {
  active: boolean;
  onClear: () => void;
}) {
  if (!active) return null;

  return (
    <Alert variant="destructive" className="animate-flash border-2">
      <AlertTriangle className="h-5 w-5" />
      <div className="flex w-full items-center justify-between">
        <div>
          <AlertTitle className="text-base font-semibold">
            Emergency Active
          </AlertTitle>
          <AlertDescription>
            Alarm is sounding on the device. Press clear to deactivate.
          </AlertDescription>
        </div>
        <Button variant="destructive" size="sm" onClick={onClear}>
          Clear Emergency
        </Button>
      </div>
    </Alert>
  );
}
