import type { MessageKey } from '../../../../shared/i18n/catalog';

export interface Preference {
  key: string;
  value: { value?: unknown };
  revision: number;
}
export interface PreferenceTransport {
  list(): Promise<Preference[]>;
  save(key: string, value: { value: unknown }, revision: number): Promise<Preference>;
}

// Один последовательный запрос на ключ; разные настройки сохраняются независимо.
export class PreferenceStore {
  private entries = new Map<string, Preference>();
  private pending = new Map<string, unknown>();
  private running = new Set<string>();
  private timers = new Map<string, ReturnType<typeof setTimeout>>();
  private listeners = new Set<() => void>();
  private version = 0;
  private keyVersions = new Map<string, number>();
  private keyValues = new Map<string, string | undefined>();
  private refreshVersion = 0;
  private disposed = false;
  error: MessageKey | '' = '';
  constructor(
    private actor: string,
    private subject: string,
    private organization: string,
    private transport: PreferenceTransport,
    entries: Preference[],
    private storage?: Storage,
    private ready = true,
  ) {
    this.replace(entries);
    if (!ready) this.error = 'preferences.unavailable';
  }
  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };
  snapshot = () => this.version;
  snapshotFor = (legacyKey: string) =>
    Math.max(this.refreshVersion, this.keyVersions.get(this.key(legacyKey)) ?? 0);
  private emit(key?: string) {
    if (this.disposed) return;
    this.version++;
    if (key) {
      // Сравниваем один раз на изменение, а не в каждой подписке React.
      const value = JSON.stringify(
        this.pending.has(key) ? this.pending.get(key) : this.entries.get(key)?.value.value,
      );
      if (value !== this.keyValues.get(key)) {
        this.keyValues.set(key, value);
        this.keyVersions.set(key, this.version);
      }
    } else this.refreshVersion = this.version;
    this.listeners.forEach((listener) => listener());
  }
  private key(legacyKey: string) {
    const prefix = `broker:registry:${encodeURIComponent(this.subject)}:`;
    const key = legacyKey.startsWith(prefix) ? legacyKey.slice(prefix.length) : '';
    return /^[a-zA-Z0-9_.:/-]{1,200}$/.test(key) ? key : '';
  }
  private cacheKey(key: string) {
    return `broker:preferences:${encodeURIComponent(this.actor)}:${this.organization}:${key}`;
  }
  private cache(key: string, value: unknown) {
    try {
      this.storage?.setItem(this.cacheKey(key), JSON.stringify(value));
    } catch {
      /* Работаем в памяти. */
    }
  }
  private replace(entries: Preference[]) {
    for (const entry of entries) {
      this.entries.set(entry.key, entry);
      if (!this.pending.has(entry.key))
        this.keyValues.set(entry.key, JSON.stringify(entry.value.value));
      if (!this.pending.has(entry.key)) this.cache(entry.key, entry.value.value);
    }
  }
  read(legacyKey: string): unknown {
    const key = this.key(legacyKey);
    if (!key) return undefined;
    if (this.pending.has(key)) return this.pending.get(key);
    if (this.entries.has(key)) return this.entries.get(key)!.value.value;
    try {
      const saved = this.storage?.getItem(this.cacheKey(key));
      if (saved !== null && saved !== undefined) return JSON.parse(saved);
      if (!this.ready) return undefined;
      // Старое хранилище не знало организацию. Переносим только в первую открытую.
      const owner = this.storage?.getItem(`broker:registry-import:${this.actor}`);
      if (!owner || owner === this.organization) {
        const legacy = this.storage?.getItem(legacyKey);
        if (legacy) return JSON.parse(legacy);
      }
    } catch {
      /* Повреждённые настройки не мешают открыть реестр. */
    }
    return undefined;
  }
  import(legacyKey: string) {
    const key = this.key(legacyKey);
    if (!key || !this.ready || this.entries.has(key) || this.pending.has(key)) return;
    const value = this.read(legacyKey);
    if (value === undefined) return;
    try {
      this.storage?.setItem(`broker:registry-import:${this.actor}`, this.organization);
    } catch {
      /* Импорт работает и без записи в localStorage. */
    }
    this.set(legacyKey, value);
  }
  set(legacyKey: string, value: unknown) {
    const key = this.key(legacyKey);
    if (!key || this.disposed) return;
    this.pending.set(key, value);
    this.cache(key, value);
    clearTimeout(this.timers.get(key));
    this.timers.set(
      key,
      setTimeout(() => {
        this.timers.delete(key);
        void this.flush(key);
      }, 350),
    );
    this.emit(key);
  }
  private async flush(key: string) {
    if (!this.ready || this.running.has(key) || !this.pending.has(key) || this.disposed) return;
    this.running.add(key);
    try {
      while (this.pending.has(key) && !this.disposed) {
        const value = this.pending.get(key);
        const entry = await this.transport.save(
          key,
          { value },
          this.entries.get(key)?.revision ?? 0,
        );
        this.entries.set(key, entry);
        if (this.pending.get(key) === value) this.pending.delete(key);
        this.emit(key);
      }
    } catch (error) {
      if ((error as { status?: number }).status === 409) {
        try {
          const current = (await this.transport.list()).find((entry) => entry.key === key);
          if (current) {
            this.pending.delete(key);
            this.entries.set(key, current);
            this.cache(key, current.value.value);
          }
          this.error = 'preferences.conflict';
        } catch {
          this.error = 'preferences.refreshFailed';
        }
      } else this.error = 'preferences.saveFailed';
      this.emit(key);
    } finally {
      this.running.delete(key);
    }
  }
  async retry() {
    try {
      const entries = await this.transport.list();
      let conflict = false;
      for (const [key, value] of this.pending) {
        const current = entries.find((entry) => entry.key === key);
        if ((current?.revision ?? 0) !== (this.entries.get(key)?.revision ?? 0)) {
          conflict ||= JSON.stringify(current?.value.value) !== JSON.stringify(value);
          this.pending.delete(key);
        }
      }
      this.replace(entries);
      this.ready = true;
      this.error = conflict ? 'preferences.remoteConflict' : '';
      this.emit();
      await Promise.all([...this.pending.keys()].map((key) => this.flush(key)));
    } catch {
      this.error = 'preferences.retryLater';
      this.emit();
    }
  }
  dispose() {
    this.disposed = true;
    this.timers.forEach(clearTimeout);
    this.listeners.clear();
  }
}
