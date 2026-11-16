// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Schlensog Audio Lab

import { defineConfig } from 'vite'
import path from 'path'
import tailwindcss from '@tailwindcss/vite'
import react from '@vitejs/plugin-react'

const isJuceBuild = process.env.VITE_JUCE_BUILD === "1";

export default defineConfig({
  base: isJuceBuild ? "./" : "/",
  build: {
    outDir: isJuceBuild ? "dist-juce" : "dist",
  },
  plugins: [
    react(),
    tailwindcss(),
  ],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },

  assetsInclude: ['**/*.svg', '**/*.csv'],
})
