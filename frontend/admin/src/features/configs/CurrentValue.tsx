import type { Workspace } from '../../app/useWorkspace';
import { pretty } from './presentation';
import { ConfigEditor } from './ConfigEditor';

export function CurrentValue({ workspace }: { workspace: Workspace }) {
  const { current, write, editor, busy, edit } = workspace;
  if (!current) return null;
  return (
    <section className="panel">
      <div className="panelhead">
        <h2>Значение</h2>
        {write && current.value !== null && !current.active_draft_id && !editor && (
          <button className="primary" disabled={busy} onClick={edit}>
            Редактировать
          </button>
        )}
        {write && current.value !== null && !current.active_draft_id && !editor && (
          <button className="danger" disabled={busy} onClick={workspace.removeConfig}>
            Удалить
          </button>
        )}
        {current.active_draft_id && (
          <span className="badge amber">Открыт драфт #{current.active_draft_id}</span>
        )}
      </div>
      {editor ? (
        <ConfigEditor workspace={workspace} />
      ) : current.value === null ? (
        <p className="muted">
          {current.version === 0
            ? 'Значение появится после подтверждения драфта.'
            : 'Конфиг удалён. Предыдущее значение можно восстановить из истории.'}
        </p>
      ) : (
        <pre>{pretty(current.value)}</pre>
      )}
    </section>
  );
}
