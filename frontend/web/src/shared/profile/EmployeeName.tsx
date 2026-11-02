import { useLocalization } from '../../features/localization/Localization';
import { useEffect, useState } from 'react';
import { request } from '../api/http';
import { type Session } from '../api/session';

export function EmployeeName({
  subject,
  session,
  fallback,
}: {
  subject?: string | null;
  session: Session;
  fallback?: string;
}) {
  const { translate } = useLocalization();
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
        if (!controller.signal.aborted)
          setName(items[0]?.display_name ?? fallback ?? translate('ui.employeeUnavailable'));
      })
      .catch(() => {
        if (!controller.signal.aborted) setName(fallback ?? translate('ui.employeeLoadError'));
      });
    return () => controller.abort();
  }, [subject, session, fallback]);
  return <>{subject ? name || translate('common.loading') : translate('ui.notAssigned')}</>;
}
