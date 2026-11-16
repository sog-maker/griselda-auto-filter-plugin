// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Schlensog Audio Lab

import { useState, useEffect, useRef, useCallback } from "react";
import { GriseldaKnob } from "./components/GriseldaKnob";
import { useJuceBridge } from "./hooks/useJuceBridge";

type KnobId =
  | "cutoff" | "highpass" | "reso" | "fmix"
  | "damount" | "dtone" | "dmix"
  | "stretch" | "grain" | "smooth" | "tmix"
  | "rate" | "depth"
  | "semitone" | "fine" | "pmix"
  | "input" | "output" | "drywet"
  | "eqlow" | "eqmid" | "eqhigh";

type SectionId = "filter" | "distortion" | "timestretch" | "lfo" | "pitch" | "eq";

const DD = {
  dist:   ["Soft Clip","Hard Clip","Tube","Foldback"],
  shape:  ["Sine","Triangle","Saw","Square","Random"],
  target: ["Cutoff","Resonance","Pitch","Distortion"],
  preset: ["Neutral Start","Wobble Trap","Dust Loop","Tape Grinder","Smoked Vinyl"],
};

const f2   = (v: number) => v.toFixed(2);
const fhz  = (v: number) => (20 + v * 19980).toFixed(2);
const fdb  = (v: number) => ((v - 0.5) * 24).toFixed(2);
const frate= (v: number) => (v * 0.5).toFixed(3);
const fgrain=(v:number) => (v * 10).toFixed(2);
const fbip = (v: number) => ((v - 0.5) * 2).toFixed(2);
const fsemi= (v: number) => Math.round((v - 0.5) * 24).toString();
const ffine= (v: number) => ((v - 0.5) * 100).toFixed(2);

function SectionHeader({
  title, enabled, onToggle
}: { title: string; enabled: boolean; onToggle: () => void }) {
  return (
    <div style={{ display:"flex", alignItems:"center", justifyContent:"space-between", marginBottom:10, position:"relative", zIndex:7 }}>
      <div style={{ fontSize:14, letterSpacing:3, color:"#d9b24c" }}>{title}</div>
      <div style={{ display:"flex", alignItems:"center", gap:12 }}>
        <div
          onClick={onToggle}
          style={{
            position:"relative", width:34, height:17, borderRadius:9, cursor:"pointer",
            background: enabled ? "linear-gradient(#caa24a,#9a7430)" : "#0a1612",
            boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.5)",
          }}
        >
          <div style={{
            position:"absolute", top:2, width:13, height:13, borderRadius:"50%",
            background:"#0d1f19", transition:"left .15s",
            left: enabled ? 19 : 2,
          }} />
        </div>
        <div style={{ fontSize:18, color:"#7a8d80", cursor:"pointer", lineHeight:"0.6" }}>—</div>
      </div>
    </div>
  );
}

function DimOverlay({ enabled }: { enabled: boolean }) {
  return (
    <div style={{
      position:"absolute", inset:0, borderRadius:10,
      background:"rgba(6,16,12,0.55)",
      opacity: enabled ? 0 : 1,
      pointerEvents: enabled ? "none" : "auto",
      transition:"opacity .18s", zIndex:6,
    }} />
  );
}

function Panel({ gridArea, children, style }: { gridArea: string; children: React.ReactNode; style?: React.CSSProperties }) {
  return (
    <div style={{
      gridArea,
      position:"relative",
      borderRadius:10,
      padding:"12px 14px",
      background:"linear-gradient(160deg,#173a30,#0e261f)",
      boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.18), 0 4px 14px rgba(0,0,0,0.3)",
      ...style,
    }}>
      {children}
    </div>
  );
}

function DDRow({ label, onClick }: { label: string; onClick: () => void }) {
  return (
    <div
      onClick={onClick}
      style={{
        display:"flex", alignItems:"center", padding:"8px 12px",
        marginBottom:12, borderRadius:5, cursor:"pointer",
        background:"#0a1713", boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.45)",
      }}
    >
      <span style={{ fontSize:13, color:"#d9b24c" }}>{label}</span>
      <span style={{ marginLeft:"auto", fontSize:11, color:"#caa24a" }}>∨</span>
    </div>
  );
}

function SpectralField({ power, knobs, texts }: { power: boolean; knobs: Record<KnobId, number>; texts: Partial<Record<string, string>> }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const rafRef = useRef<number>(0);
  const levelRef = useRef(0.4);
  const dataRef = useRef<{ base: number[]; phase: number[] } | null>(null);
  const liveRef = useRef<{ bars: number[]; level: number } | null>(null);

  useEffect(() => {
    const handler = (e: Event) => { liveRef.current = (e as CustomEvent).detail; };
    window.addEventListener("spectrum", handler);
    return () => window.removeEventListener("spectrum", handler);
  }, []);

  useEffect(() => {
    const N = 132;
    const gauss = (x: number, c: number, w: number) => Math.exp(-((x - c) ** 2) / (2 * w * w));
    let seed = 7;
    const rnd = () => { seed = (seed * 9301 + 49297) % 233280; return seed / 233280; };
    const base: number[] = [], phase: number[] = [];
    for (let i = 0; i < N; i++) {
      const x = i / (N - 1);
      let v = 0.10 + 0.22*gauss(x,0.20,0.07) + 0.30*gauss(x,0.33,0.05) + 0.95*gauss(x,0.55,0.035)
        + 0.62*gauss(x,0.50,0.02) + 0.78*gauss(x,0.64,0.045) + 0.45*gauss(x,0.78,0.05)
        + 0.30*gauss(x,0.86,0.045) + 0.16*gauss(x,0.12,0.06);
      v += (rnd() - 0.5) * 0.16;
      base.push(Math.max(0.03, v));
      phase.push(rnd() * Math.PI * 2);
    }
    dataRef.current = { base, phase };
  }, []);

  useEffect(() => {
    const draw = () => {
      const cvs = canvasRef.current;
      const data = dataRef.current;
      if (!cvs || !data) { rafRef.current = requestAnimationFrame(draw); return; }
      const ctx = cvs.getContext("2d")!;
      const W = cvs.width, H = cvs.height;
      const { base, phase } = data;
      const N = base.length;
      const t = performance.now() / 1000;
      const on = power;
      const baseY = H * 0.9;
      ctx.clearRect(0, 0, W, H);

      ctx.strokeStyle = "rgba(201,161,60,0.10)"; ctx.lineWidth = 1.4;
      for (let g = 1; g < 6; g++) { const gx = (W/6)*g; ctx.beginPath(); ctx.moveTo(gx,0); ctx.lineTo(gx,H); ctx.stroke(); }
      for (let g = 1; g < 4; g++) { const gy = (H/4)*g; ctx.beginPath(); ctx.moveTo(0,gy); ctx.lineTo(W,gy); ctx.stroke(); }

      const barW = W / N;
      const tops: number[] = [];
      for (let i = 0; i < N; i++) {
        const jitter = on ? (1 + 0.10*Math.sin(t*5+phase[i]) + 0.05*Math.sin(t*11+i)) : 0.22;
        const live = on ? liveRef.current : null;
        const v = Math.max(0.02, Math.min(1, live ? live.bars[i] : base[i] * jitter));
        tops.push(baseY - v * (H * 0.78));
      }
      for (let i = 0; i < N; i++) {
        const x = i * barW;
        const grad = ctx.createLinearGradient(0, tops[i], 0, baseY);
        grad.addColorStop(0, "rgba(214,170,60,0.55)");
        grad.addColorStop(1, "rgba(120,90,20,0.05)");
        ctx.fillStyle = grad;
        ctx.fillRect(x + barW*0.18, tops[i], barW*0.64, baseY - tops[i]);
      }
      ctx.beginPath();
      for (let i = 0; i < N; i++) {
        const x = i * barW + barW / 2;
        if (i === 0) ctx.moveTo(x, tops[i]); else ctx.lineTo(x, tops[i]);
      }
      ctx.strokeStyle = "#e9c873"; ctx.lineWidth = 2.6; ctx.lineJoin = "round";
      ctx.shadowColor = "rgba(233,200,115,0.7)"; ctx.shadowBlur = on ? 10 : 3;
      ctx.stroke(); ctx.shadowBlur = 0;

      const target = on ? (liveRef.current ? liveRef.current.level : 0.32 + 0.34*Math.abs(Math.sin(t*2.1)) + 0.16*Math.abs(Math.sin(t*6.7))) : 0;
      levelRef.current += (target - levelRef.current) * 0.2;
      const lit = Math.round(levelRef.current * 16);
      for (let i = 0; i < 16; i++) {
        const seg = document.getElementById("mseg-" + i);
        if (!seg) continue;
        if (i < lit) {
          const c = i > 12 ? "#e0b24a" : (i > 9 ? "#bfcf5a" : "#5fcf7a");
          seg.style.background = c;
          seg.style.boxShadow = `0 0 5px ${c}88, inset 0 0 0 1px rgba(201,161,60,0.2)`;
        } else {
          seg.style.background = "#13301f";
          seg.style.boxShadow = "inset 0 0 0 1px rgba(201,161,60,0.12)";
        }
      }
      rafRef.current = requestAnimationFrame(draw);
    };
    rafRef.current = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(rafRef.current);
  }, [power]);

  const statusText = `LP ${texts.cutoff ?? fhz(knobs.cutoff)}   •   TUNE ${texts.rate ?? frate(knobs.rate)}   •   DRIVE ${texts.damount ?? f2(knobs.damount)}`;

  return (
    <div style={{
      gridArea:"field",
      position:"relative", borderRadius:10, padding:"12px 16px 14px",
      background:"linear-gradient(160deg,#0c211b,#081511)",
      boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.5), 0 4px 16px rgba(0,0,0,0.4)",
      display:"flex", flexDirection:"column",
    }}>
      <div style={{ textAlign:"center", fontSize:15, letterSpacing:5, color:"#d9b24c", marginBottom:6 }}>SPECTRAL FIELD</div>
      <div style={{ position:"relative", flex:1 }}>
        <canvas ref={canvasRef} width={1320} height={940} style={{ position:"absolute", inset:0, width:"100%", height:"100%" }} />
        <div style={{ position:"absolute", right:6, top:"8%", bottom:"8%", width:13, display:"flex", flexDirection:"column-reverse", gap:3, zIndex:3 }}>
          {Array.from({ length: 16 }, (_, i) => (
            <div key={i} id={`mseg-${i}`} style={{ flex:1, borderRadius:2, background:"#13301f", boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.12)" }} />
          ))}
        </div>
      </div>
      <div style={{ textAlign:"left", fontSize:12, letterSpacing:1, color:"#9a8c5c", marginTop:6 }}>{statusText}</div>
    </div>
  );
}

export default function App() {
  const [power, setPower] = useState(true);
  const [enabled, setEnabled] = useState<Record<SectionId, boolean>>({
    filter: true, distortion: true, timestretch: true, lfo: true, pitch: true, eq: true,
  });
  const [knobs, setKnobs] = useState<Record<KnobId, number>>({
    cutoff:1.0, highpass:0.0, reso:0.10, fmix:0.0,
    damount:0.0, dtone:0.0, dmix:0.0,
    stretch:0.5, grain:0.5, smooth:0.0, tmix:0.0,
    rate:0.10, depth:0.0,
    semitone:0.5, fine:0.5, pmix:0.0,
    input:0.5, output:0.5, drywet:0.5,
    eqlow:0.5, eqmid:0.5, eqhigh:0.5,
  });
  const [dd, setDd] = useState({ dist:0, shape:0, target:0, preset:0 });
  const [texts, setTexts] = useState<Partial<Record<string, string>>>({});

  const onParamUpdate = useCallback((id: string, value: number) => {
    if (id === "power") { setPower(value > 0.5); return; }
    if (id.startsWith("on_")) { setEnabled(e => ({ ...e, [id.slice(3)]: value > 0.5 })); return; }
    if (id.startsWith("dd_")) {
      const key = id.slice(3) as keyof typeof DD;
      setDd(d => ({ ...d, [key]: Math.round(value * (DD[key].length - 1)) }));
      return;
    }
    setKnobs(k => ({ ...k, [id as KnobId]: value }));
  }, []);

  const { setParam } = useJuceBridge(onParamUpdate);

  useEffect(() => {
    const handler = (e: Event) => {
      const { id, text } = (e as CustomEvent).detail;
      setTexts(t => ({ ...t, [id]: text }));
    };
    window.addEventListener("paramText", handler);
    window.__juce__?.postMessage(JSON.stringify({ type: "READY" }));
    return () => window.removeEventListener("paramText", handler);
  }, []);

  const setKnob = useCallback((id: KnobId, v: number) => {
    setKnobs(k => ({ ...k, [id]: v }));
    setParam(id, v);
  }, [setParam]);
  const toggleSec = (id: SectionId) => {
    setEnabled(e => ({ ...e, [id]: !e[id] }));
    setParam("on_" + id, enabled[id] ? 0 : 1);
  };
  const cycleDD = (key: keyof typeof DD) => {
    const next = (dd[key] + 1) % DD[key].length;
    setDd(d => ({ ...d, [key]: next }));
    setParam("dd_" + key, next / (DD[key].length - 1));
  };

  const mk = (id: KnobId, label: string, size: number, fmt: (v:number)=>string) => ({
    id, label, size, value: knobs[id], display: texts[id] ?? fmt(knobs[id]),
    onChange: (v: number) => setKnob(id, v),
  });

  const filterKnobs = [
    mk("cutoff","Cutoff",56,fhz), mk("highpass","High Pass",56,fhz),
    mk("reso","Resonance",56,f2), mk("fmix","Mix",56,f2),
  ];
  const distKnobs = [mk("damount","Amount",64,f2), mk("dtone","Tone",64,f2), mk("dmix","Mix",64,f2)];
  const timeKnobs = [mk("stretch","Stretch",60,fbip), mk("grain","Grain",60,fgrain), mk("smooth","Smooth",60,f2), mk("tmix","Mix",60,f2)];
  const lfoKnobs  = [mk("rate","Rate",70,frate), mk("depth","Depth",70,f2)];
  const pitchKnobs= [mk("semitone","Semitone",68,fsemi), mk("fine","Fine",68,ffine), mk("pmix","Mix",68,f2)];
  const ioKnobs   = [mk("input","Input",68,fdb), mk("output","Output",68,fdb), mk("drywet","Dry/Wet",68,f2)];
  const eqKnobs   = [mk("eqlow","Low",86,fdb), mk("eqmid","Mid",86,fdb), mk("eqhigh","High",86,fdb)];

  return (
    <div style={{
      position:"relative", width:"100vw", height:"100vh", minWidth:704, minHeight:461,
      overflow:"hidden", background:"#050c0a",
    }}>
      <div style={{
        position:"absolute", left:"50%", top:"50%", width:1408, height:922,
        transform:"translate(-50%, -50%) scale(0.5)", transformOrigin:"center",
        display:"flex", alignItems:"center", justifyContent:"center",
        padding:24, background:"radial-gradient(120% 120% at 50% 0%, #0d201b, #050c0a)",
        fontFamily:"'Jost',sans-serif",
      }}>
      <div style={{
        position:"relative", width:1360, padding:24, borderRadius:14,
        background:"linear-gradient(160deg,#16352c,#0c211b)",
        boxShadow:"0 40px 90px rgba(0,0,0,0.6), inset 0 1px 0 rgba(120,170,150,0.22), inset 0 -2px 6px rgba(0,0,0,0.5)",
      }}>
        <div style={{
          position:"relative", borderRadius:8, padding:"18px 22px 22px",
          background:"radial-gradient(130% 90% at 50% 10%, #28524633, transparent 60%), linear-gradient(165deg,#1c4036,#163228 55%,#102620)",
          boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.14), inset 0 2px 30px rgba(0,0,0,0.4)",
        }}>
          <div style={{
            position:"absolute", inset:0, borderRadius:8, pointerEvents:"none",
            opacity:0.05, background:"repeating-linear-gradient(90deg,#cdeede 0 1px,transparent 1px 3px)",
            mixBlendMode:"overlay",
          }} />

          {[{top:9,left:9},{top:9,right:9},{bottom:9,left:9},{bottom:9,right:9}].map((pos, i) => (
            <div key={i} style={{
              position:"absolute", ...pos, width:18, height:18, borderRadius:"50%",
              background:"radial-gradient(circle at 35% 30%, #e7c87c, #9c7634 55%, #5c4220)",
              boxShadow:"inset 0 1px 1px rgba(255,240,200,0.6), inset 0 -1px 2px rgba(0,0,0,0.5)",
            }} />
          ))}

          <div style={{ display:"flex", alignItems:"center", gap:14, marginBottom:18, padding:"0 6px" }}>
            <div style={{
              fontFamily:"'Poiret One',sans-serif", fontSize:30, letterSpacing:5,
              color:"#e1bd64", whiteSpace:"nowrap", textShadow:"0 0 18px rgba(217,178,76,0.18)",
            }}>GRISELDA AUTO FILTER</div>
            <div style={{ flex:1 }} />
            <div
              onClick={() => cycleDD("preset")}
              style={{
                display:"flex", alignItems:"center", gap:30, padding:"8px 14px",
                minWidth:190, borderRadius:5, cursor:"pointer",
                background:"#0a1713", boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.5)",
              }}
            >
              <span style={{ fontSize:14, color:"#d9b24c" }}>{DD.preset[dd.preset]}</span>
              <span style={{ marginLeft:"auto", fontSize:11, color:"#caa24a" }}>∨</span>
            </div>
            {["EXPORT","IMPORT"].map(lbl => (
              <div key={lbl} onClick={() => setParam(lbl.toLowerCase(), 1)} style={{
                padding:"9px 18px", borderRadius:5, fontSize:13, letterSpacing:2,
                color:"#d9b24c", cursor:"pointer", boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.6)",
              }}>{lbl}</div>
            ))}
            <div
              onClick={() => { setPower(!power); setParam("power", power ? 0 : 1); }}
              style={{
                display:"flex", alignItems:"center", gap:10, padding:"9px 18px",
                borderRadius:20, cursor:"pointer", background:"#0a1713",
                boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.7)",
              }}
            >
              <span style={{
                width:10, height:10, borderRadius:"50%",
                background: power ? "#86e0a0" : "#5a2f2f",
                boxShadow: power ? "0 0 10px rgba(134,224,160,0.85)" : "none",
                display:"block",
              }} />
              <span style={{ fontSize:13, letterSpacing:2, color:"#bfae7a" }}>POWER</span>
              <span style={{ fontSize:13, letterSpacing:1, fontWeight:600, color: power ? "#8fe0a6" : "#d08a6a" }}>
                {power ? "ON" : "OFF"}
              </span>
            </div>
          </div>

          <div style={{ position:"relative" }}>
            <div style={{
              display:"grid",
              gridTemplateColumns:"300px 1fr 300px",
              gridTemplateRows:"270px 270px 184px",
              gridTemplateAreas:`"filter field lfo" "distort field pitch" "timestr eq io"`,
              gap:14,
            }}>

              <Panel gridArea="filter">
                <SectionHeader title="FILTER" enabled={enabled.filter} onToggle={() => toggleSec("filter")} />
                <div style={{ display:"grid", gridTemplateColumns:"1fr 1fr", gap:8, justifyItems:"center" }}>
                  {filterKnobs.map(k => <GriseldaKnob key={k.id} {...k} />)}
                </div>
                <DimOverlay enabled={enabled.filter} />
              </Panel>

              <Panel gridArea="distort">
                <SectionHeader title="DISTORTION" enabled={enabled.distortion} onToggle={() => toggleSec("distortion")} />
                <DDRow label={DD.dist[dd.dist]} onClick={() => cycleDD("dist")} />
                <div style={{ display:"flex", justifyContent:"space-around" }}>
                  {distKnobs.map(k => <GriseldaKnob key={k.id} {...k} />)}
                </div>
                <DimOverlay enabled={enabled.distortion} />
              </Panel>

              <SpectralField power={power} knobs={knobs} texts={texts} />

              <Panel gridArea="lfo">
                <SectionHeader title="LFO / ENV" enabled={enabled.lfo} onToggle={() => toggleSec("lfo")} />
                <div style={{ display:"flex", justifyContent:"space-around", marginBottom:12 }}>
                  {lfoKnobs.map(k => <GriseldaKnob key={k.id} {...k} />)}
                </div>
                <DDRow label={DD.shape[dd.shape]} onClick={() => cycleDD("shape")} />
                <div
                  onClick={() => cycleDD("target")}
                  style={{
                    display:"flex", alignItems:"center", padding:"7px 12px",
                    borderRadius:5, cursor:"pointer",
                    background:"#0a1713", boxShadow:"inset 0 0 0 1px rgba(201,161,60,0.45)",
                  }}
                >
                  <span style={{ fontSize:13, color:"#d9b24c" }}>{DD.target[dd.target]}</span>
                  <span style={{ marginLeft:"auto", fontSize:11, color:"#caa24a" }}>∨</span>
                </div>
                <DimOverlay enabled={enabled.lfo} />
              </Panel>

              <Panel gridArea="pitch">
                <SectionHeader title="PITCH" enabled={enabled.pitch} onToggle={() => toggleSec("pitch")} />
                <div style={{ display:"flex", justifyContent:"space-around", marginTop:18 }}>
                  {pitchKnobs.map(k => <GriseldaKnob key={k.id} {...k} />)}
                </div>
                <DimOverlay enabled={enabled.pitch} />
              </Panel>

              <Panel gridArea="timestr">
                <SectionHeader title="TIME STRETCH" enabled={enabled.timestretch} onToggle={() => toggleSec("timestretch")} />
                <div style={{ display:"flex", justifyContent:"space-around" }}>
                  {timeKnobs.map(k => <GriseldaKnob key={k.id} {...k} />)}
                </div>
                <DimOverlay enabled={enabled.timestretch} />
              </Panel>

              <Panel gridArea="eq" style={{ padding:"12px 16px" }}>
                <SectionHeader title="EQ" enabled={enabled.eq} onToggle={() => toggleSec("eq")} />
                <div style={{ display:"flex", justifyContent:"space-around", alignItems:"center" }}>
                  {eqKnobs.map(k => <GriseldaKnob key={k.id} {...k} />)}
                </div>
                <DimOverlay enabled={enabled.eq} />
              </Panel>

              <Panel gridArea="io">
                <div style={{ display:"flex", alignItems:"center", justifyContent:"space-between", marginBottom:6, position:"relative", zIndex:7 }}>
                  <div style={{ fontSize:14, letterSpacing:3, color:"#d9b24c" }}>I / O</div>
                  <div style={{ fontSize:18, color:"#7a8d80", cursor:"pointer", lineHeight:"0.6" }}>—</div>
                </div>
                <div style={{ display:"flex", justifyContent:"space-around", alignItems:"center" }}>
                  {ioKnobs.map(k => <GriseldaKnob key={k.id} {...k} />)}
                </div>
              </Panel>

            </div>

            <div style={{
              position:"absolute", inset:0, borderRadius:8,
              background:"rgba(5,12,9,0.62)", pointerEvents:"none",
              opacity: power ? 0 : 1, transition:"opacity .2s", zIndex:8,
            }} />
          </div>
        </div>
      </div>
      </div>
    </div>
  );
}
