import { useEffect, useState, type FormEvent } from 'react';
import { request, type Session } from '../../shared/api/api';
import { EditorPanel } from './EditorPanel';

type Rules = {
  number_template: string;
  reset_number_yearly: boolean;
  statuses: string[];
  route_types: string[];
  planned_containers: boolean;
  container_types: string[];
};
type Settings = {
  revision: number;
  rules: Rules;
  allowed_route_types: { code: string; name: string }[];
  allowed_container_types: string[];
  planned_containers_allowed: boolean;
};

export function ShipmentSettings({
  session,
  organization,
}: {
  session: Session;
  organization: string;
}) {
  const [settings, setSettings] = useState<Settings>();
  const [draft, setDraft] = useState<Rules>();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  useEffect(() => {
    const controller = new AbortController();
    setError('');
    request<Settings>('/api/admin/shipments/settings/get', session, {
      organization_id: organization,
    })
      .then((value) => {
        if (!controller.signal.aborted) setSettings(value);
      })
      .catch((failure) => {
        if (!controller.signal.aborted) setError(failure.message);
      });
    return () => controller.abort();
  }, [session, organization]);
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!settings || !draft || busy) return;
    setBusy(true);
    setError('');
    try {
      const result = await request<Settings>('/api/admin/shipments/settings/update', session, {
        organization_id: organization,
        expected_revision: settings.revision,
        rules: draft,
      });
      setSettings(result);
      setDraft(undefined);
      setNotice('Правила поставок сохранены');
    } catch (failure) {
      setError(failure instanceof Error ? failure.message : 'Не удалось сохранить правила');
    } finally {
      setBusy(false);
    }
  }
  const dirty = !!draft && JSON.stringify(draft) !== JSON.stringify(settings?.rules);
  function toggle(key: 'route_types' | 'container_types', code: string, checked: boolean) {
    setDraft((value) =>
      value
        ? {
            ...value,
            [key]: checked ? [...value[key], code] : value[key].filter((item) => item !== code),
          }
        : value,
    );
  }
  return (
    <section className="organization-content" role="tabpanel">
      <div className="organization-content-heading">
        <h2>Правила поставок</h2>
        <button
          className="primary"
          disabled={!settings}
          onClick={() => {
            setDraft(structuredClone(settings!.rules));
            setError('');
            setNotice('');
          }}
        >
          Изменить
        </button>
      </div>
      {error && !draft && (
        <p className="organization-error" role="alert">
          {error}
        </p>
      )}
      {notice && <p role="status">{notice}</p>}
      {!settings ? (
        <p>Загрузка настроек…</p>
      ) : (
        <dl className="shipment-rules-summary">
          <div>
            <dt>Шаблон номера</dt>
            <dd>{settings.rules.number_template}</dd>
          </div>
          <div>
            <dt>Нумерация с начала года</dt>
            <dd>{settings.rules.reset_number_yearly ? 'Да' : 'Нет'}</dd>
          </div>
          <div>
            <dt>Статусы</dt>
            <dd>{settings.rules.statuses.join(', ')}</dd>
          </div>
          <div>
            <dt>Участки маршрута</dt>
            <dd>
              {settings.allowed_route_types
                .filter((item) => settings.rules.route_types.includes(item.code))
                .map((item) => item.name)
                .join(', ') || 'Не выбраны'}
            </dd>
          </div>
          <div>
            <dt>Плановые контейнеры</dt>
            <dd>
              {settings.rules.planned_containers
                ? settings.rules.container_types.join(', ')
                : 'Отключены'}
            </dd>
          </div>
        </dl>
      )}
      {draft && settings && (
        <EditorPanel
          title="Правила поставок"
          dirty={dirty}
          busy={busy}
          close={() => {
            setDraft(undefined);
            setError('');
          }}
        >
          <form className="organization-form" onSubmit={(event) => void save(event)}>
            <fieldset className="shipment-rules-fields" disabled={busy}>
              <label>
                Шаблон номера поставки
                <input
                  required
                  maxLength={128}
                  value={draft.number_template}
                  onChange={(event) => setDraft({ ...draft, number_template: event.target.value })}
                />
              </label>
              <p className="hint">
                Переменные: {'{sequence}'}, {'{yyyy}'}, {'{yy}'}, {'{mm}'}, {'{dd}'}. Например: ABC
                {'{sequence}'}/{'{yyyy}'}.
              </p>
              <label>
                <input
                  type="checkbox"
                  checked={draft.reset_number_yearly}
                  onChange={(event) =>
                    setDraft({ ...draft, reset_number_yearly: event.target.checked })
                  }
                />
                Начинать нумерацию заново каждый год
              </label>
              <label>
                Статусы · по одному на строку
                <textarea
                  required
                  rows={4}
                  value={draft.statuses.join('\n')}
                  onChange={(event) =>
                    setDraft({ ...draft, statuses: event.target.value.split('\n') })
                  }
                />
              </label>
              <p className="hint">
                Первый статус используется при создании поставки. Изменение списка не меняет
                существующие поставки.
              </p>
              <h3>Участки маршрута</h3>
              <div className="shipment-rules-options">
                {settings.allowed_route_types.map((item) => (
                  <label key={item.code}>
                    <input
                      type="checkbox"
                      checked={draft.route_types.includes(item.code)}
                      onChange={(event) => toggle('route_types', item.code, event.target.checked)}
                    />
                    {item.name}
                  </label>
                ))}
              </div>
              <h3>Плановые контейнеры</h3>
              <label>
                <input
                  type="checkbox"
                  disabled={!settings.planned_containers_allowed}
                  checked={draft.planned_containers}
                  onChange={(event) =>
                    setDraft({ ...draft, planned_containers: event.target.checked })
                  }
                />
                Использовать плановые контейнеры
              </label>
              <div className="shipment-rules-options">
                {settings.allowed_container_types.map((code) => (
                  <label key={code}>
                    <input
                      type="checkbox"
                      checked={draft.container_types.includes(code)}
                      onChange={(event) => toggle('container_types', code, event.target.checked)}
                    />
                    {code}
                  </label>
                ))}
              </div>
            </fieldset>
            {error && (
              <p className="organization-error" role="alert">
                {error}
              </p>
            )}
            <div className="organization-editor-actions">
              <button className="primary" disabled={busy || !dirty}>
                {busy ? 'Сохраняем…' : 'Сохранить'}
              </button>
            </div>
          </form>
        </EditorPanel>
      )}
    </section>
  );
}
