import { useEffect, useState, type FormEvent } from 'react';
import { RegistryLookup } from './RegistryLookup';
import { ApiError, request, type Session } from './api';
import { useDiscardConfirmation } from './components';
import {
  counterpartyDetails,
  validateContacts,
  counterpartyPayload,
  counterpartyRoles,
  restoreCounterpartyDraft,
  type Counterparty,
  type CounterpartyDetails,
  type CounterpartyRole,
} from './counterparties';

export function CounterpartyForm({
  initial,
  session,
  onSaved,
  onError,
}: {
  initial?: Counterparty;
  session: Session;
  onSaved: (value: Counterparty) => void;
  onError: (value: unknown) => void;
}) {
  const storageKey = `broker:counterparty-draft:${session.actor}:${initial?.id ?? 'new'}`;
  const [stored] = useState(() => {
    const fallback = {
      id: initial?.id ?? crypto.randomUUID(),
      baseRevision: initial?.revision ?? 0,
      value: counterpartyDetails(initial),
    };
    try {
      return restoreCounterpartyDraft(sessionStorage.getItem(storageKey), fallback);
    } catch {
      return fallback;
    }
  });
  const [draft, setDraft] = useState(stored.value);
  const id = stored.id;
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const dirty =
    JSON.stringify(counterpartyPayload(draft)) !==
    JSON.stringify(counterpartyPayload(counterpartyDetails(initial)));
  const stale = stored.baseRevision !== (initial?.revision ?? 0) || (initial && id !== initial.id);
  useEffect(() => {
    try {
      if (dirty) sessionStorage.setItem(storageKey, JSON.stringify({ ...stored, value: draft }));
      else sessionStorage.removeItem(storageKey);
    } catch {
      /* Недоступное локальное хранилище не блокирует редактирование. */
    }
  }, [storageKey, stored, draft, dirty]);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  useEffect(() => {
    function leaving(event: BeforeUnloadEvent) {
      if (dirty || busy) {
        event.preventDefault();
        event.returnValue = '';
      }
    }
    window.addEventListener('beforeunload', leaving);
    return () => window.removeEventListener('beforeunload', leaving);
  }, [dirty, busy]);
  function set<K extends keyof CounterpartyDetails>(key: K, value: CounterpartyDetails[K]) {
    setDraft((old) => ({ ...old, [key]: value }));
  }
  function cancel() {
    if (!busy)
      confirmDiscard(dirty, () => {
        try {
          sessionStorage.removeItem(storageKey);
        } catch {
          /* Отмена доступна без хранилища. */
        }
        location.hash = initial ? `/counterparties/${initial.id}` : '/counterparties';
      });
  }
  async function submit(event: FormEvent) {
    event.preventDefault();
    if (busy || stale) return;
    setBusy(true);
    setFailure('');
    try {
      validateContacts(draft.contacts);
      const result = await request<{ counterparty: Counterparty }>(
        `/api/v1/counterparties/${initial ? 'update' : 'create'}`,
        session,
        {
          id,
          ...counterpartyPayload(draft),
          ...(initial ? { expected_revision: initial.revision } : {}),
        },
      );
      try {
        sessionStorage.removeItem(storageKey);
      } catch {
        /* Запись на сервер не зависит от хранилища. */
      }
      onSaved(result.counterparty);
    } catch (error) {
      setFailure(
        error instanceof ApiError && error.status === 409
          ? error.message + ' Проверьте существующую карточку или обновите данные перед повтором.'
          : error instanceof Error
            ? error.message
            : 'Не удалось сохранить',
      );
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <>
      <form className="counterparty-form" data-dirty={dirty} data-busy={busy} onSubmit={submit}>
        <div className="section-heading">
          <h1>{initial ? 'Редактирование контрагента' : 'Новый контрагент'}</h1>
          <button type="button" disabled={busy} onClick={cancel}>
            Отменить
          </button>
        </div>
        <p className="hint">Черновик сохраняется в этой вкладке.</p>
        {stale && (
          <p className="error" role="alert">
            Черновик основан на другой редакции. Отмените редактирование и откройте актуальную
            карточку.
          </p>
        )}
        <fieldset disabled={busy || !!stale}>
          <legend>Юридическое лицо</legend>
          <RegistryLookup
            session={session}
            draft={draft}
            onApply={(value) => setDraft((old) => ({ ...old, ...value }))}
          >
            {(lookupButton) => (
              <div className="route-fields">
                {(
                  [
                    ['name', 'Краткое наименование · обязательно', 256],
                    ['legal_name', 'Юридическое наименование', 512],
                    ['country_code', 'Код страны (RU, CN…)', 2],
                    ['tax_id', 'Налоговый номер / ИНН', 64],
                    ['kpp', 'КПП', 9],
                    ['registration_id', 'Регистрационный номер / ОГРН', 64],
                    ['legal_address', 'Юридический адрес', 1024],
                  ] as const
                ).map(([key, label, limit]) => (
                  <div
                    key={key}
                    className={
                      key === 'tax_id'
                        ? 'registry-tax-field'
                        : ['name', 'legal_name', 'legal_address'].includes(key)
                          ? 'requisites-wide-field'
                          : undefined
                    }
                  >
                    <label>
                      {label}
                      {key === 'legal_name' || key === 'legal_address' ? (
                        <textarea
                          rows={2}
                          value={draft[key]}
                          maxLength={limit}
                          onChange={(event) => set(key, event.target.value)}
                        />
                      ) : (
                        <input
                          value={draft[key]}
                          required={key === 'name' || (key === 'country_code' && !!draft.tax_id)}
                          maxLength={limit}
                          pattern={
                            key === 'kpp'
                              ? '[0-9]{9}'
                              : key === 'country_code'
                                ? '[A-Za-z]{2}'
                                : key === 'tax_id'
                                  ? '[A-Za-z0-9 -]*'
                                  : undefined
                          }
                          onChange={(event) => set(key, event.target.value)}
                        />
                      )}
                    </label>
                    {key === 'tax_id' && lookupButton}
                  </div>
                ))}
              </div>
            )}
          </RegistryLookup>
        </fieldset>
        <fieldset disabled={busy}>
          <legend>Роли</legend>
          <p className="hint">
            Роли можно сочетать. Они помогают находить компанию и не ограничивают её участие в
            перевозке.
          </p>
          <div className="role-options">
            {Object.entries(counterpartyRoles).map(([role, label]) => (
              <label key={role}>
                <input
                  type="checkbox"
                  checked={draft.roles.includes(role as CounterpartyRole)}
                  onChange={(event) =>
                    set(
                      'roles',
                      event.target.checked
                        ? [...draft.roles, role as CounterpartyRole]
                        : draft.roles.filter((value) => value !== role),
                    )
                  }
                />
                {label}
              </label>
            ))}
          </div>
        </fieldset>
        <fieldset disabled={busy}>
          <legend>Контактные лица</legend>
          {draft.contacts.map((contact, index) => (
            <div className="contact-editor" key={index}>
              <div className="route-fields">
                {(['name', 'position', 'email', 'phone'] as const).map((key) => (
                  <label key={key}>
                    {
                      {
                        name: 'Имя · обязательно',
                        position: 'Должность',
                        email: 'Email',
                        phone: 'Телефон',
                      }[key]
                    }
                    <input
                      required={key === 'name'}
                      type={key === 'email' ? 'email' : key === 'phone' ? 'tel' : 'text'}
                      title={
                        key === 'phone'
                          ? 'От 7 до 15 цифр; допустимы +, пробелы, скобки и дефисы'
                          : undefined
                      }
                      maxLength={key === 'phone' ? 64 : 256}
                      value={contact[key]}
                      onChange={(event) =>
                        set(
                          'contacts',
                          draft.contacts.map((value, i) =>
                            i === index ? { ...value, [key]: event.target.value } : value,
                          ),
                        )
                      }
                    />
                  </label>
                ))}
              </div>
              <button
                type="button"
                onClick={() =>
                  set(
                    'contacts',
                    draft.contacts.filter((_, i) => i !== index),
                  )
                }
              >
                Удалить контакт
              </button>
            </div>
          ))}
          <button
            type="button"
            disabled={draft.contacts.length >= 32}
            onClick={() =>
              set('contacts', [...draft.contacts, { name: '', position: '', email: '', phone: '' }])
            }
          >
            ＋ Добавить контакт
          </button>
        </fieldset>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <div className="form-actions">
          <button className="primary" disabled={busy || !!stale || !dirty || !draft.name.trim()}>
            {busy ? 'Сохраняем…' : initial ? 'Сохранить изменения' : 'Создать контрагента'}
          </button>
        </div>
      </form>
      {confirmation}
    </>
  );
}
