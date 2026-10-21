import { useEffect, useState, type FormEvent } from 'react';
import { ApiError, request, modes, type Session, type Transportation } from './api';
import type { Counterparty } from './CustomerSelect';
import {
  localDateTime,
  routeLeg,
  routeRequest,
  utcDateTime,
  type RouteLeg,
} from './transportation';

type Draft = { revision: number; legs: RouteLeg[] };
function emptyLeg(previous?: RouteLeg): RouteLeg {
  return {
    id: crypto.randomUUID(),
    mode: 'sea',
    origin: previous ? { ...previous.destination } : { name: '', country_code: '' },
    destination: { name: '', country_code: '' },
  };
}
function readDraft(key: string, initial: Transportation): Draft {
  try {
    const value = JSON.parse(sessionStorage.getItem(key) || 'null');
    if (
      value &&
      Number.isSafeInteger(value.revision) &&
      value.revision > 0 &&
      Array.isArray(value.legs) &&
      value.legs.length <= 32 &&
      value.legs.every(
        (leg: RouteLeg) =>
          leg &&
          typeof leg.id === 'string' &&
          ['road', 'rail', 'sea'].includes(leg.mode) &&
          [leg.origin, leg.destination].every(
            (point) =>
              point && typeof point.name === 'string' && typeof point.country_code === 'string',
          ) &&
          [leg.contractor_id, leg.planned_departure_at, leg.planned_arrival_at].every(
            (field) => field === undefined || typeof field === 'string',
          ),
      )
    )
      return value;
  } catch {
    /* Повреждённый локальный черновик не мешает открыть серверный маршрут. */
  }
  return {
    revision: initial.revision,
    legs: initial.legs.length ? initial.legs.map(routeLeg) : [emptyLeg()],
  };
}
export function RouteEditor({
  initial,
  session,
  customers,
  allowed,
  onSaved,
  onError,
}: {
  initial: Transportation;
  session: Session;
  customers: Counterparty[];
  allowed: boolean;
  onSaved: (value: Transportation) => void;
  onError: (error: unknown) => void;
}) {
  const key = `broker:route-draft:${session.actor}:${initial.id}`;
  const [draft, setDraft] = useState(() => readDraft(key, initial));
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const dirty = JSON.stringify(draft.legs) !== JSON.stringify(initial.legs.map(routeLeg));
  const stale = draft.revision !== initial.revision;
  useEffect(() => {
    if (!allowed) return;
    try {
      sessionStorage.setItem(key, JSON.stringify(draft));
    } catch {
      /* Хранилище может быть запрещено браузером. */
    }
  }, [key, draft, allowed]);
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
  function update(index: number, value: RouteLeg) {
    setDraft((old) => ({ ...old, legs: old.legs.map((leg, i) => (i === index ? value : leg)) }));
  }
  function move(index: number, delta: number) {
    setDraft((old) => {
      const legs = [...old.legs];
      [legs[index], legs[index + delta]] = [legs[index + delta], legs[index]];
      return { ...old, legs };
    });
  }
  function discard() {
    if (busy || (dirty && !confirm('Отменить изменения маршрута?'))) return;
    try {
      sessionStorage.removeItem(key);
    } catch {
      /* Отмена доступна и без локального хранилища. */
    }
    location.hash = `/transportations/${initial.id}`;
  }
  async function submit(event: FormEvent) {
    event.preventDefault();
    if (!allowed || stale) return;
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ transportation: Transportation }>(
        '/api/v1/transportation-routes/set',
        session,
        routeRequest(initial.id, draft.revision, draft.legs),
      );
      try {
        sessionStorage.removeItem(key);
      } catch {
        /* Результат записи не зависит от локального хранилища. */
      }
      onSaved(result.transportation);
    } catch (error) {
      setFailure(
        error instanceof ApiError && error.status === 409
          ? 'Перевозка изменилась. Отмените локальное редактирование, обновите карточку и проверьте актуальный маршрут.'
          : error instanceof Error
            ? error.message
            : 'Не удалось сохранить маршрут',
      );
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  if (!allowed)
    return (
      <div className="panel-content">
        <p>Редактирование недоступно: проверьте право на изменение и наличие фактических дат.</p>
        <a href={`#/transportations/${initial.id}`}>Вернуться к перевозке</a>
      </div>
    );
  return (
    <form className="route-editor panel-content" onSubmit={submit}>
      <div className="section-heading">
        <div>
          <h2>Редактирование маршрута</h2>
          <p className="hint">
            Черновик сохраняется только в этой вкладке. Время:{' '}
            {Intl.DateTimeFormat().resolvedOptions().timeZone}.
          </p>
        </div>
        <button type="button" disabled={busy} onClick={discard}>
          Отменить
        </button>
      </div>
      {stale && (
        <p className="error" role="alert">
          Черновик основан на редакции {draft.revision}, на сервере уже {initial.revision}. Отмените
          редактирование, чтобы открыть актуальные данные.
        </p>
      )}
      <fieldset disabled={busy || stale}>
        {draft.legs.map((leg, index) => (
          <section className="route-leg" key={leg.id}>
            <div className="section-heading">
              <h3>Участок {index + 1}</h3>
              <div className="actions">
                <button
                  type="button"
                  aria-label={`Поднять участок ${index + 1}`}
                  disabled={!index}
                  onClick={() => move(index, -1)}
                >
                  ↑
                </button>
                <button
                  type="button"
                  aria-label={`Опустить участок ${index + 1}`}
                  disabled={index === draft.legs.length - 1}
                  onClick={() => move(index, 1)}
                >
                  ↓
                </button>
                <button
                  type="button"
                  onClick={() =>
                    setDraft((old) => ({ ...old, legs: old.legs.filter((_, i) => i !== index) }))
                  }
                >
                  Удалить
                </button>
              </div>
            </div>
            <div className="route-fields">
              <label>
                Транспорт
                <select
                  value={leg.mode}
                  onChange={(event) =>
                    update(index, { ...leg, mode: event.target.value as RouteLeg['mode'] })
                  }
                >
                  {Object.entries(modes).map(([value, title]) => (
                    <option key={value} value={value}>
                      {title}
                    </option>
                  ))}
                </select>
              </label>
              <label>
                Подрядчик
                <select
                  value={leg.contractor_id || ''}
                  onChange={(event) =>
                    update(index, { ...leg, contractor_id: event.target.value || undefined })
                  }
                >
                  <option value="">Не выбран</option>
                  {leg.contractor_id &&
                    !customers.some((item) => item.id === leg.contractor_id) && (
                      <option value={leg.contractor_id}>Недоступный контрагент</option>
                    )}
                  {customers.map((item) => (
                    <option key={item.id} value={item.id}>
                      {item.name}
                    </option>
                  ))}
                </select>
              </label>
              {(['origin', 'destination'] as const).map((point) => (
                <div key={point} className="point-fields">
                  <label>
                    {point === 'origin' ? 'Откуда' : 'Куда'}
                    <input
                      required
                      maxLength={256}
                      value={leg[point].name}
                      onChange={(event) =>
                        update(index, {
                          ...leg,
                          [point]: { ...leg[point], name: event.target.value },
                        })
                      }
                    />
                  </label>
                  <label>
                    Код страны
                    <input
                      required
                      pattern="[A-Za-z]{2}"
                      minLength={2}
                      maxLength={2}
                      placeholder="RU"
                      value={leg[point].country_code}
                      onChange={(event) =>
                        update(index, {
                          ...leg,
                          [point]: {
                            ...leg[point],
                            country_code: event.target.value.toUpperCase(),
                          },
                        })
                      }
                    />
                  </label>
                </div>
              ))}
              {(['planned_departure_at', 'planned_arrival_at'] as const).map((field) => (
                <label key={field}>
                  {field === 'planned_departure_at' ? 'Плановое отправление' : 'Плановое прибытие'}
                  <input
                    type="datetime-local"
                    value={localDateTime(leg[field])}
                    onChange={(event) =>
                      update(index, {
                        ...leg,
                        [field]: event.target.value ? utcDateTime(event.target.value) : undefined,
                      })
                    }
                  />
                </label>
              ))}
            </div>
          </section>
        ))}
        <button
          type="button"
          disabled={draft.legs.length >= 32}
          onClick={() =>
            setDraft((old) => ({ ...old, legs: [...old.legs, emptyLeg(old.legs.at(-1))] }))
          }
        >
          ＋ Добавить участок
        </button>
      </fieldset>
      {!draft.legs.length && <p className="hint">Добавьте хотя бы один участок.</p>}
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <div className="form-actions">
        <button className="primary" disabled={busy || stale || !dirty || !draft.legs.length}>
          {busy ? 'Сохраняем…' : 'Сохранить маршрут'}
        </button>
      </div>
    </form>
  );
}
