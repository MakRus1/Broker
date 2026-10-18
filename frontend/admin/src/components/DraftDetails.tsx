import type { Workspace } from '../hooks/useWorkspace';
import { pretty, statusName } from '../presentation';
import { canApprove } from '../api';

export function DraftDetails({ workspace }: { workspace: Workspace }) {
  const { draft, session, owner, write, busy, edit, run, transition, events } = workspace;
  if (!draft || !session) return null;
  return (
    <section className="panel">
      <div className="panelhead">
        <h2>Драфт #{draft.id}</h2>
        <span className={'badge ' + (draft.status === 'draft' ? 'amber' : '')}>
          {statusName[draft.status]}
        </span>
      </div>
      <p>{draft.description}</p>
      <p className="muted">
        Автор: {draft.author} · Версия драфта {draft.version} · Основан на версии{' '}
        {draft.base_version}
      </p>
      <div className="comparison">
        <div>
          <h3>Было</h3>
          <pre>{pretty(draft.base_value)}</pre>
        </div>
        <div>
          <h3>Предложено</h3>
          <pre>{pretty(draft.value)}</pre>
        </div>
      </div>
      <div className="actions">
        {owner && write && (
          <>
            <button disabled={busy} onClick={edit}>
              Изменить драфт
            </button>
            <button
              className="danger"
              disabled={busy}
              onClick={() => run(() => transition('cancel'))}
            >
              Отменить драфт
            </button>
          </>
        )}
        {canApprove(session, draft) && (
          <button
            className="primary"
            disabled={busy}
            onClick={() => run(() => transition('approve'))}
          >
            Подтвердить и опубликовать
          </button>
        )}
      </div>
      {owner && <p className="muted">Подтвердить этот драфт должен другой пользователь.</p>}
      {events.length > 0 && (
        <details>
          <summary>События драфта ({events.length})</summary>
          <ol>
            {events.map((event, index) => (
              <li key={index}>
                <strong>{event.action}</strong> · {event.actor} · {event.created_at}
              </li>
            ))}
          </ol>
        </details>
      )}
    </section>
  );
}
