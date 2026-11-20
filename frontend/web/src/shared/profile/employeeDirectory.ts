import { request } from '../api/http';
import type { Session } from '../api/session';

export interface Employee {
  subject: string;
  display_name: string;
  username?: string;
}

// Bound to the session object: names are not persisted or shared between sessions.
const directories = new WeakMap<Session, Map<string, Employee>>();
export function rememberEmployees(session: Session, employees: Employee[]) {
  let directory = directories.get(session);
  if (!directory) directories.set(session, (directory = new Map()));
  for (const employee of employees) directory.set(employee.subject, employee);
}
export function cachedEmployee(session: Session, subject: string) {
  return directories.get(session)?.get(subject);
}
export async function loadEmployee(session: Session, subject: string, signal: AbortSignal) {
  const result = await request<{ items: Employee[] }>(
    '/api/organizations/employees/list',
    session,
    { subject, limit: 1 },
    signal,
  );
  if (!signal.aborted) rememberEmployees(session, result.items);
  return result.items.find((employee) => employee.subject === subject);
}
