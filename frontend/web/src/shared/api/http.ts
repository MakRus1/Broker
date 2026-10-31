import type { Session } from './session';
export class ApiError extends Error {
  constructor(
    public status: number,
    message: string,
    public traceId?: string,
  ) {
    super(message);
  }
}
export async function request<T>(
  path: string,
  session?: Session,
  body?: unknown,
  signal?: AbortSignal,
): Promise<T> {
  const response = await fetch(path, {
    method: body === undefined ? 'GET' : 'POST',
    credentials: 'same-origin',
    headers:
      body === undefined
        ? {}
        : {
            'Content-Type': 'application/json',
            'X-CSRF-Token': session?.csrf_token ?? '',
          },
    body: body === undefined ? undefined : JSON.stringify(body),
    signal,
  });
  const value = await response.json().catch(() => null);
  if (!response.ok) {
    throw new ApiError(
      response.status,
      value?.message ?? 'Не удалось выполнить запрос. Попробуйте ещё раз.',
      response.headers.get('X-Trace-Id') ?? undefined,
    );
  }
  return value as T;
}
