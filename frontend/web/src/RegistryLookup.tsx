import { useEffect, useRef, useState, type ReactNode } from 'react';
import { request, type Session } from './api';
import type { CounterpartyDetails } from './counterparties';
import { isInn, registryRequisites, type RegistryParty, type RegistryRequisites } from './registry';

export function RegistryLookup({
  session,
  draft,
  onApply,
  children,
}: {
  session: Session;
  draft: CounterpartyDetails;
  onApply: (value: RegistryRequisites) => void;
  children: (lookupButton: ReactNode) => ReactNode;
}) {
  const taxId = draft.tax_id.trim();
  const country = draft.country_code.trim().toUpperCase();
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [message, setMessage] = useState('');
  const pending = useRef<AbortController | undefined>(undefined);
  const requestNumber = useRef(0);
  useEffect(() => {
    ++requestNumber.current;
    pending.current?.abort();
    setFailure('');
    setMessage('');
    setBusy(false);
    return () => {
      ++requestNumber.current;
      pending.current?.abort();
    };
  }, [taxId, country]);
  async function lookup() {
    if (busy) return;
    if (!isInn(taxId)) {
      setFailure('Проверьте ИНН: нужны 10 или 12 цифр с верной контрольной суммой.');
      return;
    }
    const controller = new AbortController();
    pending.current = controller;
    const number = ++requestNumber.current;
    setBusy(true);
    setFailure('');
    setMessage('');
    try {
      const result = await request<{ items: RegistryParty[] }>(
        '/api/v1/counterparties/lookup',
        session,
        { tax_id: taxId },
        controller.signal,
      );
      if (number !== requestNumber.current) return;
      if (result.items[0]) {
        onApply(registryRequisites(result.items[0]));
        setMessage('Реквизиты подставлены из DaData. Проверьте данные перед сохранением.');
      }
      if (!result.items.length)
        setMessage('Организация или ИП с таким ИНН не найдены. Можно заполнить реквизиты вручную.');
    } catch (error) {
      if (!controller.signal.aborted && number === requestNumber.current)
        setFailure(error instanceof Error ? error.message : 'Не удалось получить реквизиты');
    } finally {
      if (number === requestNumber.current) setBusy(false);
    }
  }
  return (
    <section className="registry-lookup" aria-label="Поиск реквизитов по ИНН">
      {children(
        <button
          type="button"
          className="registry-search-button"
          title="Найти головную организацию или ИП России в DaData"
          disabled={busy || !taxId || (!!country && country !== 'RU')}
          onClick={() => void lookup()}
        >
          {busy ? 'Ищем…' : 'Заполнить по ИНН'}
        </button>,
      )}
      {!!country && country !== 'RU' && (
        <p className="hint">Для иностранных компаний реквизиты заполняются вручную.</p>
      )}
      {message && <p role="status">{message}</p>}
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
    </section>
  );
}
