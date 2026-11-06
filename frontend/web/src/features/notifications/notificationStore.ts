export type NotificationLevel = 'success' | 'warning' | 'error' | 'info';
export interface NotificationInput {
  id?: string;
  remote?: boolean;
  message: string;
  level: NotificationLevel;
  route?: string;
  action?: { label: string; run: () => void | Promise<void> };
}
export interface NotificationItem extends NotificationInput {
  id: string;
  createdAt: number;
  read: boolean;
  toast: boolean;
  resolved: boolean;
}
export class NotificationStore {
  private items: NotificationItem[] = [];
  private remoteRead?: (ids: string[], dismiss?: boolean) => Promise<void>;
  private pendingRead = new Set<string>();
  private hidden = new Set<string>();
  private remoteInitialized = false;
  private seenRemote = new Set<string>();
  setRemoteRead = (read: (ids: string[], dismiss?: boolean) => Promise<void>) => {
    this.remoteRead = read;
  };
  mergeRemote = (incoming: NotificationItem[]) => {
    const known = new Set(incoming.map((item) => item.id));
    incoming.filter((item) => item.read).forEach((item) => this.pendingRead.delete(item.id));
    const previous = new Map(this.items.map((item) => [item.id, item]));
    const merged = [
      ...incoming
        .filter((item) => !this.hidden.has(item.id))
        .map((item) => ({
          ...item,
          read: item.read || this.pendingRead.has(item.id),
          toast:
            !(item.read || this.pendingRead.has(item.id)) &&
            (previous.get(item.id)?.toast ??
              (this.remoteInitialized && !this.seenRemote.has(item.id))),
        })),
      ...this.items.filter((item) => !known.has(item.id)),
    ]
      .sort((a, b) => b.createdAt - a.createdAt)
      .slice(0, 100);
    incoming.forEach((item) => this.seenRemote.add(item.id));
    this.remoteInitialized = true;
    let toasts = 0;
    this.update(
      merged.map((item) => (item.toast && ++toasts > 3 ? { ...item, toast: false } : item)),
    );
  };
  clearRemote = () => {
    this.remoteInitialized = false;
    this.seenRemote.clear();
    this.update(this.items.filter((item) => !item.remote));
  };
  private listeners = new Set<() => void>();
  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };
  snapshot = () => this.items;
  private update(items: NotificationItem[]) {
    this.items = items;
    this.listeners.forEach((listener) => listener());
  }
  publish = (input: NotificationInput) => {
    const id = input.id ?? crypto.randomUUID();
    const old = this.items.find((item) => item.id === id);
    if (old && !old.resolved && old.message === input.message && old.level === input.level)
      return id;
    this.update(
      [
        { ...input, id, createdAt: Date.now(), read: false, toast: true, resolved: false },
        ...this.items.filter((item) => item.id !== id),
      ].slice(0, 100),
    );
    return id;
  };
  hideToast = (id: string) =>
    this.update(this.items.map((item) => (item.id === id ? { ...item, toast: false } : item)));
  markRead = (id?: string) => {
    const ids = this.items
      .filter((item) => item.remote && !item.read && (!id || item.id === id))
      .map((item) => item.id);
    ids.forEach((value) => this.pendingRead.add(value));
    if (ids.length && this.remoteRead)
      void this.remoteRead(ids).catch(() => {
        ids.forEach((value) => this.pendingRead.delete(value));
        this.update(
          this.items.map((item) => (ids.includes(item.id) ? { ...item, read: false } : item)),
        );
      });
    this.update(
      this.items.map((item) =>
        !id || item.id === id ? { ...item, read: true, toast: false } : item,
      ),
    );
  };
  resolve = (id: string) => {
    if (!this.items.some((item) => item.id === id && !item.resolved)) return;
    this.update(
      this.items.map((item) =>
        item.id === id
          ? { ...item, resolved: true, read: true, toast: false, action: undefined }
          : item,
      ),
    );
  };
  clearRead = () => {
    const removed = this.items.filter((item) => item.read && item.remote);
    removed.forEach((item) => this.hidden.add(item.id));
    this.update(this.items.filter((item) => !item.read));
    if (removed.length && this.remoteRead)
      void this.remoteRead(
        removed.map((item) => item.id),
        true,
      ).catch(() => {
        removed.forEach((item) => this.hidden.delete(item.id));
        this.mergeRemote(removed);
      });
  };
}
