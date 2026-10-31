import { useEffect, useState, type FormEvent } from 'react';
import { request, type Session } from '../../shared/api/api';
import { Select } from '../../shared/ui/Select';
import { EditorPanel } from './EditorPanel';
import { currenciesWithCurrent } from '../../../../shared/currencies';
type Settings = {
  reporting_currency: string;
  revision: number;
  outgoing_number_template: string;
  incoming_number_template: string;
  reset_numbers_yearly: boolean;
};
export function FinanceSettings({
  session,
  organization,
}: {
  session: Session;
  organization: string;
}) {
  const [settings, setSettings] = useState<Settings>();
  const [draft, setDraft] = useState<Settings>();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  useEffect(() => {
    setSettings(undefined);
    setDraft(undefined);
    setError('');
    setNotice('');
    const abort = new AbortController();
    request<Settings>(
      '/api/admin/finance/settings/get',
      session,
      { organization_id: organization },
      abort.signal,
    )
      .then((value) => {
        if (!abort.signal.aborted) setSettings(value);
      })
      .catch((failure) => {
        if (!abort.signal.aborted) setError(failure.message);
      });
    return () => abort.abort();
  }, [session, organization]);
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!settings || !draft || busy) return;
    setBusy(true);
    setError('');
    try {
      const saved = await request<Settings>('/api/admin/finance/settings/update', session, {
        organization_id: organization,
        reporting_currency: draft.reporting_currency,
        outgoing_number_template: draft.outgoing_number_template,
        incoming_number_template: draft.incoming_number_template,
        reset_numbers_yearly: draft.reset_numbers_yearly,
        expected_revision: settings.revision,
      });
      setSettings(saved);
      setDraft(undefined);
      setNotice('Настройки финансов сохранены');
    } catch (failure) {
      setError((failure as Error).message);
    } finally {
      setBusy(false);
    }
  }
  return (
    <section className="organization-content" role="tabpanel">
      <div className="organization-content-heading">
        <h2>Финансы</h2>
        <button disabled={!settings} onClick={() => setDraft(settings!)}>
          Редактировать
        </button>
      </div>
      {!draft && error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
      {notice && (
        <p className="hint" role="status">
          {notice}
        </p>
      )}
      {!settings ? (
        <p className="hint">Загружаем настройки…</p>
      ) : (
        <dl className="organization-details">
          <div>
            <dt>Валюта пересчёта</dt>
            <dd>
              {
                currenciesWithCurrent(settings.reporting_currency).find(
                  (item) => item.value === settings.reporting_currency,
                )?.label
              }
            </dd>
          </div>
          <div>
            <dt>Исходящие счета</dt>
            <dd>{settings.outgoing_number_template}</dd>
          </div>
          <div>
            <dt>Входящие счета</dt>
            <dd>{settings.incoming_number_template}</dd>
          </div>
          <div>
            <dt>Сброс счётчиков</dt>
            <dd>{settings.reset_numbers_yearly ? 'Каждый год' : 'Не сбрасываются'}</dd>
          </div>
        </dl>
      )}
      <p className="hint">
        Используется в сводках поставок и перевозок. Валюты и суммы выставленных счетов сохраняются.
      </p>
      {draft !== undefined && (
        <EditorPanel
          title="Настройки финансов"
          dirty={JSON.stringify(draft) !== JSON.stringify(settings)}
          busy={busy}
          close={() => setDraft(undefined)}
        >
          <form onSubmit={save}>
            <fieldset disabled={busy} className="organization-editor-fields">
              <Select
                label="Валюта пересчёта"
                value={draft.reporting_currency}
                options={currenciesWithCurrent(draft.reporting_currency)}
                onChange={(reporting_currency) => setDraft({ ...draft, reporting_currency })}
              />
              <label>
                Маска исходящих счетов
                <input
                  required
                  maxLength={100}
                  value={draft.outgoing_number_template}
                  onChange={(e) => setDraft({ ...draft, outgoing_number_template: e.target.value })}
                />
              </label>
              <label>
                Маска входящих счетов
                <input
                  required
                  maxLength={100}
                  value={draft.incoming_number_template}
                  onChange={(e) => setDraft({ ...draft, incoming_number_template: e.target.value })}
                />
              </label>
              <label className="check">
                <input
                  type="checkbox"
                  checked={draft.reset_numbers_yearly}
                  onChange={(e) => setDraft({ ...draft, reset_numbers_yearly: e.target.checked })}
                />
                Начинать счётчики заново каждый год
              </label>
              <p className="hint">
                Обязательная переменная — {'{sequence}'}. Дата документа: {'{yyyy}'}, {'{yy}'},{' '}
                {'{mm}'}, {'{dd}'}. Номер можно ввести вручную.
              </p>
            </fieldset>
            {error && (
              <p className="error" role="alert">
                {error}
              </p>
            )}
            <div className="actions">
              <button type="button" disabled={busy} onClick={() => setDraft(undefined)}>
                Отменить
              </button>
              <button className="primary" disabled={busy}>
                Сохранить
              </button>
            </div>
          </form>
        </EditorPanel>
      )}
    </section>
  );
}
