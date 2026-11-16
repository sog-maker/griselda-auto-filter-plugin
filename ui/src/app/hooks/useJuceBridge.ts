// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Schlensog Audio Lab

import { useCallback, useEffect } from "react";

declare global {
  interface Window {
    __juce__?: {
      postMessage: (msg: string) => void;
    };
  }
}

type ParamUpdateDetail = {
  id: string;
  value: number;
};

const isInJuce = () => typeof window !== "undefined" && !!window.__juce__;

export function useJuceBridge(
  onParamUpdate: (id: string, value: number) => void
) {
  useEffect(() => {
    const handleParamUpdate = (event: Event) => {
      const rawDetail = (event as CustomEvent<ParamUpdateDetail | string>).detail;
      let detail: ParamUpdateDetail;

      try {
        detail = typeof rawDetail === "string" ? JSON.parse(rawDetail) : rawDetail;
      } catch {
        return;
      }

      if (
        !detail ||
        typeof detail.id !== "string" ||
        typeof detail.value !== "number" ||
        !Number.isFinite(detail.value)
      ) {
        return;
      }

      onParamUpdate(detail.id, Math.max(0, Math.min(1, detail.value)));
    };

    window.addEventListener("paramUpdate", handleParamUpdate);
    return () => window.removeEventListener("paramUpdate", handleParamUpdate);
  }, [onParamUpdate]);

  const setParam = useCallback((id: string, value: number) => {
    if (isInJuce()) {
      window.__juce__?.postMessage(
        JSON.stringify({
          type: "SET_PARAM",
          id,
          value: Math.max(0, Math.min(1, value)),
        })
      );
    }
  }, []);

  return { setParam, isInJuce: isInJuce() };
}
