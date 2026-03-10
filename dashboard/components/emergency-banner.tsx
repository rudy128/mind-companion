// =============================================================
// Emergency Banner — Full-width flashing alert
// =============================================================
"use client";

export function EmergencyBanner({
  active,
  onClear,
}: {
  active: boolean;
  onClear: () => void;
}) {
  if (!active) return null;

  return (
    <div className="animate-flash rounded-xl border-2 border-red-500 bg-red-500/20 px-6 py-4 flex items-center justify-between">
      <div className="flex items-center gap-3">
        <span className="text-3xl">🚨</span>
        <div>
          <p className="text-lg font-bold text-red-400">EMERGENCY ACTIVE</p>
          <p className="text-sm text-red-300/80">
            Alarm is sounding on the device
          </p>
        </div>
      </div>
      <button
        onClick={onClear}
        className="rounded-lg bg-red-600 px-4 py-2 text-sm font-semibold text-white hover:bg-red-700 transition-colors cursor-pointer"
      >
        Clear Emergency
      </button>
    </div>
  );
}
