import { useEffect, useState } from 'react';
import { request } from '../api/http';
import { type Session } from '../api/session';

export function EmployeeName({ subject, session }: { subject?: string | null; session: Session }) {
  const [name, setName] = useState('');
  useEffect(() => {
    setName('');
    if (!subject) return;
    const controller = new AbortController();
    request<{ items: { subject: string; display_name: string }[] }>(
      '/api/organizations/employees/list',
      session,
      { subject, limit: 1 },
      controller.signal,
    )
      .then(({ items }) => {
        if (!controller.signal.aborted) setName(items[0]?.display_name ?? 'Сотрудник недоступен');
      })
      .catch(() => {
        if (!controller.signal.aborted) setName('Не удалось загрузить сотрудника');
      });
    return () => controller.abort();
  }, [subject, session]);
  return <>{subject ? name || 'Загрузка…' : 'Не назначен'}</>;
}
