import { useEffect, useState, type FormEvent } from 'react';
import { request, type Session } from '../../api';
import { Select } from '../Select';
import { EditorPanel } from './EditorPanel';
import { currenciesWithCurrent } from '../../../../shared/currencies';
type Settings = { reporting_currency: string; revision: number };
export function FinanceSettings({
  session,
  organization,
}: {
  session: Session;
  organization: string;
}) {
  const [settings, setSettings] = useState<Settings>();
  const [draft, setDraft] = useState<string>();
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
        reporting_currency: draft,
        expected_revision: settings.revision,
      });
      setSettings(saved);
      setDraft(undefined);
      setNotice('Валюта пересчёта сохранена');
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
        <button disabled={!settings} onClick={() => setDraft(settings!.reporting_currency)}>
          Редактировать
        </button>
      </div>
      {error && (
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
        </dl>
      )}
      <p className="hint">
        Используется в сводках поставок и перевозок. Валюты и суммы выставленных счетов сохраняются.
      </p>
      {draft !== undefined && (
        <EditorPanel
          title="Настройки финансов"
          dirty={draft !== settings?.reporting_currency}
          busy={busy}
          close={() => setDraft(undefined)}
        >
          <form onSubmit={save}>
            <fieldset disabled={busy} className="organization-editor-fields">
              <Select
                label="Валюта пересчёта"
                value={draft}
                options={currenciesWithCurrent(draft)}
                onChange={setDraft}
              />
            </fieldset>
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
