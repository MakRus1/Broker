import { useEffect, useState, type FormEvent } from 'react';
import { request, type Session } from '../../shared/api/api';
import { EditorPanel } from './EditorPanel';
type Settings = { number_template: string; reset_number_yearly: boolean; revision: number };
export function ContractNumberingSettings({
  session,
  organization,
}: {
  session: Session;
  organization: string;
}) {
  const [settings, setSettings] = useState<Settings>();
  const [draft, setDraft] = useState<Settings>();
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);
  useEffect(() => {
    const abort = new AbortController();
    request<Settings>(
      '/api/admin/contracts/numbering/get',
      session,
      { organization_id: organization },
      abort.signal,
    )
      .then((value) => {
        if (!abort.signal.aborted) setSettings(value);
      })
      .catch((e) => {
        if (!abort.signal.aborted) setError(e.message);
      });
    return () => abort.abort();
  }, [session, organization]);
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!draft || busy) return;
    setBusy(true);
    setError('');
    try {
      const value = await request<Settings>('/api/admin/contracts/numbering/update', session, {
        organization_id: organization,
        number_template: draft.number_template,
        reset_number_yearly: draft.reset_number_yearly,
        expected_revision: draft.revision,
      });
      setSettings(value);
      setDraft(undefined);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  return (
    <section className="organization-content">
      <div className="organization-content-heading">
        <h2>Нумерация договоров</h2>
        <button disabled={!settings} onClick={() => setDraft(settings)}>
          Редактировать
        </button>
      </div>
      {!draft && error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
      {settings && (
        <dl className="organization-details">
          <div>
            <dt>Маска номера</dt>
            <dd>{settings.number_template}</dd>
          </div>
          <div>
            <dt>Сброс счётчика</dt>
            <dd>{settings.reset_number_yearly ? 'Каждый год' : 'Не сбрасывается'}</dd>
          </div>
        </dl>
      )}
      {draft && (
        <EditorPanel
          title="Нумерация договоров"
          dirty={JSON.stringify(draft) !== JSON.stringify(settings)}
          busy={busy}
          close={() => setDraft(undefined)}
        >
          <form onSubmit={save}>
            <fieldset disabled={busy} className="organization-editor-fields">
              <label>
                Маска номера
                <input
                  required
                  maxLength={128}
                  value={draft.number_template}
                  onChange={(e) => setDraft({ ...draft, number_template: e.target.value })}
                />
              </label>
              <label className="check">
                <input
                  type="checkbox"
                  checked={draft.reset_number_yearly}
                  onChange={(e) => setDraft({ ...draft, reset_number_yearly: e.target.checked })}
                />
                Начинать счётчик заново каждый год
              </label>
              <p className="hint">
                Обязательно {'{sequence}'}. Дата договора: {'{yyyy}'}, {'{yy}'}, {'{mm}'}, {'{dd}'}.
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
