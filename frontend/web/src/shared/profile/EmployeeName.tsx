import { useLocalization } from '../../features/localization/Localization';
import { useEffect, useState } from 'react';
import { cachedEmployee, loadEmployee } from './employeeDirectory';
import { type Session } from '../api/session';

export function EmployeeName({
  subject,
  session,
  fallback,
  format = 'name',
}: {
  subject?: string | null;
  session: Session;
  fallback?: string;
  format?: 'name' | 'login';
}) {
  const { translate } = useLocalization();
  const caption = (employee: ReturnType<typeof cachedEmployee>) =>
    employee
      ? format === 'login'
        ? employee.username || translate('ui.loginUnavailable')
        : employee.display_name
      : '';
  const [name, setName] = useState(() =>
    caption(subject ? cachedEmployee(session, subject) : undefined),
  );
  useEffect(() => {
    setName(caption(subject ? cachedEmployee(session, subject) : undefined));
    if (!subject) return;
    const controller = new AbortController();
    loadEmployee(session, subject, controller.signal)
      .then((employee) => {
        if (!controller.signal.aborted)
          setName(
            caption(employee) ||
              (format === 'login'
                ? translate('ui.loginUnavailable')
                : (fallback ?? translate('ui.employeeUnavailable'))),
          );
      })
      .catch(() => {
        if (!controller.signal.aborted)
          setName(
            format === 'login'
              ? translate('ui.loginUnavailable')
              : (fallback ?? translate('ui.employeeLoadError')),
          );
      });
    return () => controller.abort();
  }, [subject, session, fallback, format]);
  return <>{subject ? name || translate('common.loading') : translate('ui.notAssigned')}</>;
}
