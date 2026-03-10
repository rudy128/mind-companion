// =============================================================
// Reusable Card wrapper for dashboard widgets
// =============================================================
"use client";

import type { ReactNode } from "react";

export function Card({
  title,
  icon,
  children,
  className = "",
  accent,
}: {
  title: string;
  icon: string;
  children: ReactNode;
  className?: string;
  accent?: string;
}) {
  return (
    <div
      className={`rounded-2xl border border-card-border bg-card p-5 ${className}`}
    >
      <div className="mb-3 flex items-center gap-2">
        <span className="text-lg">{icon}</span>
        <h3 className={`text-sm font-semibold uppercase tracking-wider ${accent ?? "text-muted"}`}>
          {title}
        </h3>
      </div>
      {children}
    </div>
  );
}
