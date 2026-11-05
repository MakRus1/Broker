// Compile registryPageCache.ts to CommonJS first; pass its output path as the first argument.
// node --expose-gc benchmarks/cache-memory.cjs /tmp/broker-cache-memory/registryPageCache.js
const { RegistryPageCache } = require(process.argv[2]);
if (!global.gc) throw new Error('Run Node with --expose-gc');
const maxRows = Number(process.argv[3] ?? 100000);
if (!Number.isInteger(maxRows) || maxRows <= 0 || maxRows % 50)
  throw new Error('Row count must be a positive multiple of 50');
const caches = Array.from({ length: 4 }, () => new RegistryPageCache());
const checkpoints = new Set([1000, 10000, 25000, 50000, maxRows].filter((n) => n <= maxRows));
const samples = [];
const started = performance.now();
function sample(rows, phase = 'append') {
  global.gc();
  const value = {
    phase,
    rowsPerRegistry: rows,
    heapUsedBytes: process.memoryUsage().heapUsed,
    resident: caches.map((cache) => cache.items.length),
    elapsedMs: Math.round(performance.now() - started),
  };
  samples.push(value);
  process.stderr.write(JSON.stringify(value) + '\n');
}
sample(0);
for (let start = 0; start < maxRows; start += 50) {
  for (let registry = 0; registry < 4; registry++) {
    const cache = caches[registry];
    const items = JSON.parse(
      JSON.stringify(
        Array.from({ length: 50 }, (_, i) => ({
          id: String(start + i),
          cells: Array.from(
            { length: 100 },
            (_, col) => `Registry ${registry} / row ${start + i} / column ${col} / value`,
          ),
        })),
      ),
    );
    cache.append({ items, next_cursor: start + 50 < maxRows ? start + 50 : undefined });
    cache.protect([start]);
    cache.evict();
  }
  if (checkpoints.has(start + 50)) sample(start + 50);
}
for (let round = 1; round <= 3; round++) {
  for (let start = maxRows - 50; start >= 0; start -= 1000) {
    for (let registry = 0; registry < 4; registry++) {
      const cache = caches[registry];
      const items = JSON.parse(
        JSON.stringify(
          Array.from({ length: 50 }, (_, i) => ({
            id: String(start + i),
            cells: Array.from(
              { length: 100 },
              (_, col) => `Registry ${registry} / row ${start + i} / column ${col} / value`,
            ),
          })),
        ),
      );
      cache.restore(start / 50, {
        items,
        next_cursor: start + 50 < maxRows ? start + 50 : undefined,
      });
      cache.protect([start]);
      cache.evict();
    }
  }
  sample(maxRows, `restore-round-${round}`);
}
console.log(
  JSON.stringify(
    {
      runtime: process.version,
      platform: process.platform,
      registries: 4,
      columns: 100,
      pageSize: 50,
      pageLimit: 8,
      samples,
    },
    null,
    2,
  ),
);
