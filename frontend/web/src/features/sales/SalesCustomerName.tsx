import { useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { useLocalization } from '../localization/Localization';
export function SalesCustomerName({ id, session }: { id: string; session: Session }) {
  const { translate } = useLocalization();
  const [name, setName] = useState('');
  useEffect(() => {
    const controller = new AbortController();
    request<{ counterparty: { name: string } }>(
      '/api/v1/counterparties/get',
      session,
      { id },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) setName(result.counterparty.name);
      })
      .catch(() => {
        if (!controller.signal.aborted) setName(translate('filter.unavailable'));
      });
    return () => controller.abort();
  }, [id, session, translate]);
  return <a href={`#/counterparties/${id}`}>{name || translate('common.loading')}</a>;
}
