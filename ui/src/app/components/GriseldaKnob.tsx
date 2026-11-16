// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Schlensog Audio Lab

import { useRef, useCallback } from "react";

interface GriseldaKnobProps {
  label: string;
  value: number;
  display: string;
  size?: number;
  onChange?: (v: number) => void;
}

export function GriseldaKnob({ label, value, display, size = 76, onChange }: GriseldaKnobProps) {
  const startRef = useRef<{ y: number; v: number } | null>(null);

  const onPointerDown = useCallback((e: React.PointerEvent) => {
    if (!onChange) return;
    e.preventDefault();
    startRef.current = { y: e.clientY, v: value };
    const move = (ev: PointerEvent) => {
      if (!startRef.current) return;
      const v = Math.max(0, Math.min(1, startRef.current.v + (startRef.current.y - ev.clientY) / 200));
      onChange(v);
    };
    const up = () => {
      startRef.current = null;
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  }, [value, onChange]);

  const rot = `rotate(${(-135 + value * 270).toFixed(1)}deg)`;

  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 6, fontFamily: "'Jost', sans-serif" }}>
      <div style={{ fontSize: 12, letterSpacing: 1, color: "#cba24a", whiteSpace: "nowrap" }}>{label}</div>
      <div
        onPointerDown={onPointerDown}
        style={{
          position: "relative",
          width: size,
          height: size,
          borderRadius: "50%",
          cursor: "ns-resize",
          background: "linear-gradient(160deg,#2a4a40,#0a1813)",
          boxShadow: "0 6px 14px rgba(0,0,0,0.5), inset 0 2px 3px rgba(150,200,180,0.22), inset 0 -3px 7px rgba(0,0,0,0.6)",
          flexShrink: 0,
        }}
      >
        <div style={{
          position: "absolute", inset: "6%", borderRadius: "50%",
          background: "conic-gradient(from 220deg, #6e5224, #c79a4e, #5a4220, #c79a4e, #6e5224)",
          boxShadow: "inset 0 0 5px rgba(0,0,0,0.4)",
        }} />
        <div style={{
          position: "absolute", inset: "11%", borderRadius: "50%",
          background: "radial-gradient(circle at 36% 28%, #3d6356, #1d3a31 58%, #112520)",
          boxShadow: "inset 0 3px 5px rgba(150,200,180,0.26), inset 0 -5px 10px rgba(0,0,0,0.5)",
        }} />
        <div style={{ position: "absolute", inset: 0, transform: rot }}>
          <div style={{
            position: "absolute", left: "50%", top: "11%", width: 3, height: "25%",
            marginLeft: -1.5, borderRadius: 2,
            background: "linear-gradient(#f3da8c,#c79a40)",
            boxShadow: "0 0 6px rgba(217,178,76,0.55)",
          }} />
        </div>
      </div>
      <div style={{
        minWidth: 56, textAlign: "center", padding: "3px 7px", borderRadius: 3,
        background: "#0a1612", fontSize: 11, letterSpacing: 0.5, color: "#cba24a",
        boxShadow: "inset 0 0 0 1px rgba(201,161,60,0.35)",
      }}>{display}</div>
    </div>
  );
}
