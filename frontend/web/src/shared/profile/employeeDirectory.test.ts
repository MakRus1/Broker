import { describe, expect, it, vi } from 'vitest';
import type { Session } from '../api/session';
import { request } from '../api/http';
import { cachedEmployee, loadEmployee, rememberEmployees } from './employeeDirectory';

vi.mock('../api/http', () => ({ request: vi.fn() }));
const session = (): Session => ({
  actor: 'author',
  subject: 'author',
  organization_id: 'org',
  display_name: 'author',
  csrf_token: '',
  permissions: [],
});
const employee = { subject: 'employee', display_name: 'Employee', username: 'login' };

describe('employee captions', () => {
  it('isolates captions between sessions', () => {
    const first = session();
    const second = session();
    rememberEmployees(first, [employee]);
    expect(cachedEmployee(first, employee.subject)).toEqual(employee);
    expect(cachedEmployee(second, employee.subject)).toBeUndefined();
  });
  it('refreshes a cached caption from the directory', async () => {
    const current = session();
    rememberEmployees(current, [employee]);
    const changed = { ...employee, display_name: 'Updated' };
    vi.mocked(request).mockResolvedValueOnce({ items: [changed] });
    await loadEmployee(current, employee.subject, new AbortController().signal);
    expect(cachedEmployee(current, employee.subject)).toEqual(changed);
  });
  it('does not cache an aborted response', async () => {
    const current = session();
    const controller = new AbortController();
    vi.mocked(request).mockResolvedValueOnce({ items: [employee] });
    controller.abort();
    await loadEmployee(current, employee.subject, controller.signal);
    expect(cachedEmployee(current, employee.subject)).toBeUndefined();
  });
});
