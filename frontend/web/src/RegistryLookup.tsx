import { useEffect, useRef, useState, type ReactNode } from 'react';
import { request, type Session } from './api';
import type { CounterpartyDetails } from './counterparties';
import {
  isInn,
  registryFields,
  registryRequisites,
  registryStatus,
  type RegistryParty,
  type RegistryRequisites,
} from './registry';

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
  const [found, setFound] = useState<RegistryParty>();
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [message, setMessage] = useState('');
  const pending = useRef<AbortController | undefined>(undefined);
  const requestNumber = useRef(0);
  useEffect(() => {
    ++requestNumber.current;
    pending.current?.abort();
    setFound(undefined);
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
    setFound(undefined);
    try {
      const result = await request<{ items: RegistryParty[] }>(
        '/api/v1/counterparties/lookup',
        session,
        { tax_id: taxId },
        controller.signal,
      );
      if (number !== requestNumber.current) return;
      setFound(result.items[0]);
      if (!result.items.length)
        setMessage('Организация или ИП с таким ИНН не найдены. Можно заполнить реквизиты вручную.');
    } catch (error) {
      if (!controller.signal.aborted && number === requestNumber.current)
        setFailure(error instanceof Error ? error.message : 'Не удалось получить реквизиты');
    } finally {
      if (number === requestNumber.current) setBusy(false);
    }
  }
  const values = found ? registryRequisites(found) : undefined;
  const changed = values
    ? (Object.keys(registryFields) as (keyof RegistryRequisites)[]).filter(
        (key) => (draft[key] ?? '').trim() !== values[key],
      )
    : [];
  const replacing = changed.some((key) => Boolean((draft[key] ?? '').trim()));
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
          {busy ? 'Ищем…' : 'Найти реквизиты'}
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
      {found && values && (
        <div className="registry-preview">
          <div className="section-heading">
            <h3>Найденные реквизиты</h3>
            <span>{registryStatus(found.status)}</span>
          </div>
          {found.status !== 'ACTIVE' && (
            <p className="registry-warning">
              Проверьте статус организации перед использованием реквизитов.
            </p>
          )}
          <dl>
            {(Object.entries(registryFields) as [keyof RegistryRequisites, string][]).map(
              ([key, label]) => (
                <div key={key} className={changed.includes(key) ? 'registry-field-changed' : ''}>
                  <dt>{label}</dt>
                  <dd>
                    {values[key] || '—'}
                    {changed.includes(key) && Boolean((draft[key] ?? '').trim()) && (
                      <small>Сейчас: {draft[key]}</small>
                    )}
                  </dd>
                </div>
              ),
            )}
          </dl>
          {replacing && (
            <p className="registry-warning">
              Заполненные поля с отличиями будут заменены. Контактные лица и роли сохранятся.
            </p>
          )}
          <button
            type="button"
            className="primary"
            disabled={!changed.length}
            onClick={() => {
              onApply(values);
              setFound(undefined);
              setMessage('Реквизиты подставлены в форму. Проверьте их и сохраните карточку.');
            }}
          >
            Подставить в форму
          </button>
        </div>
      )}
    </section>
  );
}
