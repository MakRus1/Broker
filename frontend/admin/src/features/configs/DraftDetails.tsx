import type { Workspace } from '../../app/useWorkspace';
import { statusName } from './presentation';
import { changes } from './changes';
import { canApprove } from '../../shared/api/api';
import { Actor } from '../../shared/ui/Actor';
import { ValueDiff } from './ValueDiff';

const actions: Record<string, string> = {
  create: 'Создан',
  edit: 'Изменён',
  approve: 'Подтверждён и опубликован',
  cancel: 'Отменён',
};

export function DraftDetails({ workspace }: { workspace: Workspace }) {
  const { draft, session, owner, write, busy, edit, run, transition, events, current, restore } =
    workspace;
  if (!draft || !session) return null;
  return (
    <section className="panel">
      <div className="panelhead">
        <h2>
          Драфт #{draft.id}
          {draft.value === null ? ' · Удаление конфига' : ''}
        </h2>
        <span className={'badge ' + (draft.status === 'draft' ? 'amber' : '')}>
          {statusName[draft.status]}
        </span>
      </div>
      <p>{draft.description}</p>
      <p className="muted">
        Автор: <Actor actor={draft.author} name={draft.author_name} session={session} /> · Версия
        драфта {draft.version} ·{' '}
        {draft.base_value === null
          ? 'Первоначальное создание'
          : `На основе версии ${draft.base_version}`}
      </p>
      {draft.approved_by && (
        <p className="muted">
          Подтвердил:{' '}
          <Actor actor={draft.approved_by} name={draft.approved_by_name} session={session} />
        </p>
      )}
      <ValueDiff before={draft.base_value} after={draft.value} />
      <div className="actions">
        {owner && write && (
          <>
            <button disabled={busy || draft.value === null} onClick={edit}>
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
        {canApprove(session, draft, current?.allow_self_approval) && (
          <button
            className="primary"
            disabled={busy}
            onClick={() => run(() => transition('approve'))}
          >
            Подтвердить и опубликовать
          </button>
        )}
        {write &&
          !current?.active_draft_id &&
          draft.status === 'published' &&
          draft.value !== null && (
            <button
              disabled={busy || changes(current?.value, draft.value).length === 0}
              onClick={() => restore(draft.value, `Откат к значению из драфта #${draft.id}`)}
            >
              Откатить
            </button>
          )}
      </div>
      {owner && !current?.allow_self_approval && (
        <p className="muted">Подтвердить этот драфт должен другой пользователь.</p>
      )}
      {draft.status === 'published' && write && (
        <p className="muted">
          «Откатить» подставит значение этой версии в новый драфт. Публикация — после подтверждения.
        </p>
      )}
      {events.length > 0 && (
        <details>
          <summary>События драфта ({events.length})</summary>
          <ol>
            {events.map((event, index) => (
              <li key={index}>
                <strong>{actions[event.action] ?? event.action}</strong> ·{' '}
                <Actor
                  actor={event.actor}
                  name={
                    event.action === 'approve'
                      ? event.snapshot.approved_by_name
                      : event.snapshot.author_name
                  }
                  session={session}
                />{' '}
                · {new Date(event.created_at).toLocaleString('ru-RU')}
              </li>
            ))}
          </ol>
        </details>
      )}
    </section>
  );
}
