import type { Workspace } from '../hooks/useWorkspace';
import { pretty } from '../presentation';
import { ConfigEditor } from './ConfigEditor';

export function CurrentValue({ workspace }: { workspace: Workspace }) {
  const { current, write, editor, busy, edit } = workspace;
  if (!current) return null;
  return (
    <section className="panel">
      <div className="panelhead">
        <h2>Значение</h2>
        {write && !current.active_draft_id && !editor && (
          <button className="primary" disabled={busy} onClick={edit}>
            Редактировать
          </button>
        )}
        {current.active_draft_id && (
          <span className="badge amber">Открыт драфт #{current.active_draft_id}</span>
        )}
      </div>
      {editor ? <ConfigEditor workspace={workspace} /> : <pre>{pretty(current.value)}</pre>}
    </section>
  );
}
