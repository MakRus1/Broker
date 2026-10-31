import { request, ApiError } from './http';
import type { Session } from './session';
// Один ключ сохраняется для повторной отправки той же команды после сетевой ошибки.
export class Commands {
  private keys = new Map<string, { key: string; body: object }>();
  async send<T>(path: string, session: Session, body: object): Promise<T> {
    const { expected_revision: _revision, ...intent } = body as Record<string, unknown>;
    const fingerprint = path + JSON.stringify(intent);
    const command = this.keys.get(fingerprint) ?? { key: crypto.randomUUID(), body };
    this.keys.set(fingerprint, command);
    try {
      const result = await request<T>(path, session, {
        ...command.body,
        idempotency_key: command.key,
      });
      this.keys.delete(fingerprint);
      return result;
    } catch (value) {
      if (value instanceof ApiError && value.status >= 400 && value.status < 500)
        this.keys.delete(fingerprint);
      throw value;
    }
  }
}
