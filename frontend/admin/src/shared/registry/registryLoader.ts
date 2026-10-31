export interface RegistryCursor {
  id: string;
  value: string;
  scope: string;
}
export interface RegistryPage<T> {
  items: T[];
  next_cursor?: RegistryCursor;
}
export interface RegistryState<T> extends RegistryPage<T> {
  loading: boolean;
  loadingMore: boolean;
  error: string;
}
export function emptyRegistry<T>(): RegistryState<T> {
  return { items: [], loading: false, loadingMore: false, error: '' };
}

export class RegistryLoader<T> {
  state = emptyRegistry<T>();
  private generation = 0;
  private controller?: AbortController;
  private query: object = {};
  constructor(
    private fetchPage: (body: object, signal: AbortSignal) => Promise<RegistryPage<T>>,
    private changed: (state: RegistryState<T>) => void,
  ) {}
  private update(state: RegistryState<T>) {
    this.state = state;
    this.changed(state);
  }
  stop() {
    ++this.generation;
    this.controller?.abort();
  }
  reset() {
    this.stop();
    this.update(emptyRegistry<T>());
  }
  async start(query: object) {
    this.stop();
    this.query = query;
    this.update({ ...emptyRegistry<T>(), loading: true });
    await this.load(false);
  }
  async more() {
    if (this.state.loading || this.state.loadingMore) return;
    if (!this.state.next_cursor) {
      if (this.state.error) await this.start(this.query);
      return;
    }
    this.update({ ...this.state, loadingMore: true, error: '' });
    await this.load(true);
  }
  private async load(append: boolean) {
    const current = this.generation;
    const controller = new AbortController();
    this.controller = controller;
    const cursor = append ? this.state.next_cursor : undefined;
    try {
      const value = await this.fetchPage(
        { ...this.query, limit: 50, ...(cursor ? { cursor } : {}) },
        controller.signal,
      );
      if (current !== this.generation) return;
      this.update({
        ...value,
        items: append ? [...this.state.items, ...value.items] : value.items,
        loading: false,
        loadingMore: false,
        error: '',
      });
    } catch (reason) {
      if (current !== this.generation || controller.signal.aborted) return;
      this.update({
        ...this.state,
        loading: false,
        loadingMore: false,
        error: reason instanceof Error ? reason.message : 'Не удалось загрузить список',
      });
    }
  }
}
