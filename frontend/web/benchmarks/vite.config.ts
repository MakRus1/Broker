import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath } from 'node:url';
export default defineConfig({
  root: fileURLToPath(new URL('.', import.meta.url)),
  plugins: [react()],
  publicDir: fileURLToPath(new URL('../public', import.meta.url)),
  build: {
    outDir: '/tmp/broker-registry-benchmark',
    emptyOutDir: true,
    rollupOptions: {
      input: {
        registry: fileURLToPath(new URL('./registry.html', import.meta.url)),
        pages: fileURLToPath(new URL('./pages.html', import.meta.url)),
        endurance: fileURLToPath(new URL('./endurance.html', import.meta.url)),
      },
    },
  },
});
