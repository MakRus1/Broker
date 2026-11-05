import { performance } from 'node:perf_hooks';
import { PreferenceStore } from '../src/features/preferences/preferenceStore.ts';

// 30 открытых вкладок, по 4 настройки; в каждой — 100 описаний столбцов.
const prefix = 'broker:registry:author:';
const entries = Array.from({ length: 120 }, (_, index) => ({
  key: `registry-${index}:columns:v1`,
  revision: 1,
  value: {
    value: Array.from({ length: 100 }, (_, column) => ({
      id: `column-${column}`,
      visible: true,
      width: 180,
    })),
  },
}));
const store = new PreferenceStore(
  'author',
  'author',
  'organization',
  {
    list: async () => entries,
    save: async () => {
      throw new Error('Read-only benchmark');
    },
  },
  entries,
);
const keys = entries.map((entry) => prefix + entry.key);
let result;
function measure(run) {
  for (let i = 0; i < 100; i++) run();
  const samples = [];
  for (let sample = 0; sample < 9; sample++) {
    const start = performance.now();
    for (let i = 0; i < 500; i++) run();
    samples.push((performance.now() - start) / 500);
  }
  return samples.sort((a, b) => a - b)[4];
}
const before = measure(() => {
  for (const key of keys) result = JSON.stringify(store.read(key));
});
const after = measure(() => {
  for (const key of keys) result = store.snapshotFor(key);
});
console.table({ before: { milliseconds: before }, after: { milliseconds: after } });
console.log(`Snapshot CPU improvement: ${(before / after).toFixed(1)}x`);
void result;
