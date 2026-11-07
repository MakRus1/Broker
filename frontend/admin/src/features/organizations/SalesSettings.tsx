import { useEffect, useState, type FormEvent } from 'react';
import { request, type Session } from '../../shared/api/api';
type Stage = { id: string; name: string; enabled: boolean };
type Settings = {
  revision: number;
  stages: Stage[];
  inquiry_number_template: string;
  reset_number_yearly: boolean;
};
export function SalesSettings({
  session,
  organization,
}: {
  session: Session;
  organization: string;
}) {
  const [settings, setSettings] = useState<Settings>();
  const [draft, setDraft] = useState<Stage[]>([]);
  const [numberTemplate, setNumberTemplate] = useState('ENQ-{sequence}');
  const [yearly, setYearly] = useState(false);
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);
  useEffect(() => {
    const controller = new AbortController();
    request<Settings>('/api/admin/sales/settings/get', session, { organization_id: organization })
      .then((result) => {
        if (!controller.signal.aborted) {
          setSettings(result);
          setDraft(result.stages);
          setNumberTemplate(result.inquiry_number_template);
          setYearly(result.reset_number_yearly);
        }
      })
      .catch((e) => {
        if (!controller.signal.aborted) setError(e.message);
      });
    return () => controller.abort();
  }, [session, organization]);
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!settings || busy) return;
    setBusy(true);
    setError('');
    try {
      const result = await request<Settings>('/api/admin/sales/settings/update', session, {
        organization_id: organization,
        expected_revision: settings.revision,
        stages: draft,
        inquiry_number_template: numberTemplate,
        reset_number_yearly: yearly,
      });
      setSettings(result);
      setDraft(result.stages);
      setNumberTemplate(result.inquiry_number_template);
      setYearly(result.reset_number_yearly);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Не удалось сохранить этапы');
    } finally {
      setBusy(false);
    }
  }
  function move(index: number, delta: number) {
    setDraft((old) => {
      const next = [...old];
      [next[index], next[index + delta]] = [next[index + delta], next[index]];
      return next;
    });
  }
  return (
    <section className="organization-content">
      <h2>Продажи</h2>
      {error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
      <form className="organization-form" onSubmit={save}>
        <fieldset className="organization-editor-fields" disabled={busy || !settings}>
          <section className="sales-numbering">
            <h3>Нумерация входящих запросов</h3>
            <label className="sales-stage-name">
              Шаблон номера
              <input
                required
                maxLength={128}
                value={numberTemplate}
                onChange={(event) => setNumberTemplate(event.target.value)}
              />
            </label>
            <p className="hint">
              Обязателен {'{sequence}'} — порядковый номер. Также доступны {'{yyyy}'}, {'{yy}'},{' '}
              {'{mm}'}, {'{dd}'}. Шаблон применяется только к новым запросам.
            </p>
            <label className="checkbox">
              <input
                type="checkbox"
                checked={yearly}
                onChange={(event) => setYearly(event.target.checked)}
              />
              Начинать нумерацию заново каждый год
            </label>
          </section>
          <h3>Этапы продаж</h3>
          <p className="hint">
            Порядок этапов используется в воронке. Отключённые этапы остаются в существующих
            карточках.
          </p>
          {draft.map((stage, index) => (
            <div className="sales-stage-row" key={stage.id}>
              <label className="sales-stage-name">
                Название этапа
                <input
                  required
                  maxLength={100}
                  value={stage.name}
                  onChange={(e) =>
                    setDraft((old) =>
                      old.map((s) => (s.id === stage.id ? { ...s, name: e.target.value } : s)),
                    )
                  }
                />
              </label>
              <label className="checkbox">
                <input
                  type="checkbox"
                  checked={stage.enabled}
                  onChange={(e) =>
                    setDraft((old) =>
                      old.map((s) => (s.id === stage.id ? { ...s, enabled: e.target.checked } : s)),
                    )
                  }
                />
                Доступен для выбора
              </label>
              <div className="sales-stage-order">
                <button
                  aria-label={`Поднять этап «${stage.name}»`}
                  title="Поднять этап"
                  type="button"
                  disabled={index === 0}
                  onClick={() => move(index, -1)}
                >
                  <svg aria-hidden="true" width="18" height="18" viewBox="0 0 24 24">
                    <path
                      d="m6 14 6-6 6 6"
                      fill="none"
                      stroke="currentColor"
                      strokeWidth="2"
                      strokeLinecap="round"
                      strokeLinejoin="round"
                    />
                  </svg>
                </button>
                <button
                  type="button"
                  aria-label={`Опустить этап «${stage.name}»`}
                  title="Опустить этап"
                  disabled={index === draft.length - 1}
                  onClick={() => move(index, 1)}
                >
                  <svg aria-hidden="true" width="18" height="18" viewBox="0 0 24 24">
                    <path
                      d="m6 10 6 6 6-6"
                      fill="none"
                      stroke="currentColor"
                      strokeWidth="2"
                      strokeLinecap="round"
                      strokeLinejoin="round"
                    />
                  </svg>
                </button>
              </div>
            </div>
          ))}
          <div className="actions">
            <button
              type="button"
              onClick={() =>
                setDraft((old) => [...old, { id: crypto.randomUUID(), name: '', enabled: true }])
              }
            >
              Добавить этап
            </button>
            <button
              className="primary"
              disabled={
                !settings ||
                (JSON.stringify(draft) === JSON.stringify(settings.stages) &&
                  numberTemplate === settings.inquiry_number_template &&
                  yearly === settings.reset_number_yearly)
              }
            >
              Сохранить настройки
            </button>
          </div>
        </fieldset>
      </form>
    </section>
  );
}
