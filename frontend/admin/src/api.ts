export type JsonObject = Record<string, unknown>;
export interface Session {
  actor: string;
  subject: string;
  display_name?: string;
  csrf_token: string;
  permissions: string[];
}
export interface CatalogEntry {
  config_name: string;
  version: number;
  active_draft_id: number | null;
}
export interface Current {
  allow_self_approval?: boolean;
  config_name: string;
  value: JsonObject | null;
  version: number;
  active_draft_id?: number;
}
export interface Draft {
  id: number;
  config_name: string;
  author: string;
  author_name?: string;
  approved_by_name?: string;
  description: string;
  version: number;
  status: 'draft' | 'published' | 'cancelled';
  base_version: number;
  base_value: JsonObject | null;
  value: JsonObject | null;
  approved_by?: string;
  published_revision?: number;
}
export interface DraftEvent {
  action: string;
  actor: string;
  created_at: string;
  snapshot: Omit<Draft, 'id'>;
}
export class ApiError extends Error {
  constructor(
    public status: number,
    message: string,
  ) {
    super(message);
  }
}
export async function request<T>(path: string, session?: Session, body?: unknown): Promise<T> {
  const response = await fetch(path, {
    method: body === undefined ? 'GET' : 'POST',
    credentials: 'same-origin',
    headers:
      body === undefined
        ? {}
        : { 'Content-Type': 'application/json', 'X-CSRF-Token': session?.csrf_token ?? '' },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  let value;
  try {
    value = await response.json();
  } catch {
    throw new ApiError(response.status, 'Сервис вернул некорректный ответ. Повторите позже.');
  }
  if (!response.ok)
    throw new ApiError(response.status, value.message ?? 'Не удалось выполнить запрос');
  return value as T;
}
export function parseValue(text: string): JsonObject {
  const value: unknown = JSON.parse(text);
  if (value === null || typeof value !== 'object' || Array.isArray(value))
    throw new Error('Значение конфига должно быть JSON-объектом');
  return value as JsonObject;
}
export function canApprove(
  session: Pick<Session, 'actor' | 'permissions'>,
  draft: Pick<Draft, 'author' | 'status'>,
  allowSelfApproval = false,
): boolean {
  return (
    draft.status === 'draft' &&
    (allowSelfApproval || draft.author !== session.actor) &&
    session.permissions.includes('configs.approve')
  );
}
