export interface RegistryPage<T, Cursor> {
  items: T[];
  next_cursor?: Cursor | null;
}
interface StoredPage<T, Cursor> {
  cursor: Cursor | undefined;
  next: Cursor | undefined;
  start: number;
  ids: string[];
  items?: T[];
}
export class RegistryPageChanged extends Error {}

// Only identities/cursors survive eviction. Payload is bounded by pages near the viewport.
export class RegistryPageCache<T extends { id: string }, Cursor> {
  readonly pages: StoredPage<T, Cursor>[] = [];
  rows: { id: string }[] = [];
  protectedPages = new Set<number>();
  private readonly identities = new Set<string>();
  constructor(readonly limit = 8) {}
  get more() {
    return !!this.pages.length && this.pages.at(-1)!.next !== undefined;
  }
  get next() {
    return this.pages.at(-1)?.next;
  }
  append(page: RegistryPage<T, Cursor>) {
    if (
      page.next_cursor != null &&
      (!page.items.length || JSON.stringify(page.next_cursor) === JSON.stringify(this.next))
    )
      throw new RegistryPageChanged('Registry cursor did not advance');
    if (
      page.items.some((item) => this.identities.has(item.id)) ||
      new Set(page.items.map((item) => item.id)).size !== page.items.length
    )
      throw new RegistryPageChanged('Registry page identities changed');
    for (const item of page.items) this.identities.add(item.id);
    this.pages.push({
      cursor: this.next,
      next: page.next_cursor ?? undefined,
      start: this.rows.length,
      ids: page.items.map((item) => item.id),
      items: page.items,
    });
    this.rows = [...this.rows, ...page.items.map(({ id }) => ({ id }))];
  }
  validate(index: number, page: RegistryPage<T, Cursor>) {
    const stored = this.pages[index];
    if (
      !stored ||
      stored.ids.length !== page.items.length ||
      stored.ids.some((id, i) => id !== page.items[i].id) ||
      JSON.stringify(stored.next) !== JSON.stringify(page.next_cursor ?? undefined)
    )
      throw new RegistryPageChanged('Registry page identities changed');
  }
  restore(index: number, page: RegistryPage<T, Cursor>) {
    this.validate(index, page);
    this.pages[index].items = page.items;
  }
  async readRows(
    ids: ReadonlySet<string>,
    fetch: (cursor: Cursor | undefined) => Promise<RegistryPage<T, Cursor>>,
    signal: AbortSignal,
  ) {
    const selected: T[] = [];
    for (let index = 0; index < this.pages.length; index++) {
      const stored = this.pages[index];
      if (!stored.ids.some((id) => ids.has(id))) continue;
      if (signal.aborted) throw new Error('Selection request aborted');
      const page = stored.items
        ? { items: stored.items, next_cursor: stored.next }
        : await fetch(stored.cursor);
      if (signal.aborted) throw new Error('Selection request aborted');
      this.validate(index, page);
      selected.push(...page.items.filter((item) => ids.has(item.id)));
    }
    if (selected.length !== ids.size) throw new RegistryPageChanged('Registry selection changed');
    return selected;
  }
  pageAt(row: number) {
    let low = 0,
      high = this.pages.length - 1;
    while (low < high) {
      const mid = Math.ceil((low + high) / 2);
      if (this.pages[mid].start <= row) low = mid;
      else high = mid - 1;
    }
    return low;
  }
  get(row: number) {
    const page = this.pages[this.pageAt(row)];
    return page?.items?.[row - page.start];
  }
  protect(indices: readonly number[]) {
    this.protectedPages = new Set<number>();
    for (const row of indices) {
      const page = this.pageAt(row);
      for (let i = Math.max(0, page - 1); i <= Math.min(this.pages.length - 1, page + 1); i++)
        this.protectedPages.add(i);
    }
  }
  evict() {
    let count = this.pages.filter((page) => page.items).length;
    const center = [...this.protectedPages][0] ?? this.pages.length - 1;
    const candidates = this.pages
      .map((_, i) => i)
      .sort((a, b) => Math.abs(b - center) - Math.abs(a - center));
    for (const index of candidates) {
      const page = this.pages[index];
      if (count <= this.limit) break;
      if (!page.items || this.protectedPages.has(index)) continue;
      page.items = undefined;
      count--;
    }
  }
  get items() {
    return this.pages.flatMap((page) => page.items ?? []);
  }
}
export interface RegistryPageSource<T extends { id: string }> {
  rows: readonly { id: string }[];
  get: (index: number) => T | undefined;
  ensure: (indices: readonly number[]) => void;
  failed: (index: number) => boolean;
  retry: (index: number) => void;
}
