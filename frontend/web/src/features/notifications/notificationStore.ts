export type NotificationLevel = 'success' | 'warning' | 'error';
export interface NotificationInput {
  id?: string;
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
  markRead = (id?: string) =>
    this.update(
      this.items.map((item) =>
        !id || item.id === id ? { ...item, read: true, toast: false } : item,
      ),
    );
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
  clearRead = () => this.update(this.items.filter((item) => !item.read));
}
