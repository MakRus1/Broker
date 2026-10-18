import type { Workspace } from '../hooks/useWorkspace';
import { statusName } from '../presentation';

export function DraftHistory({ workspace }: { workspace: Workspace }) {
  const { drafts, busy, editor, setDraft } = workspace;
  return (
    <section className="panel">
      <div className="panelhead">
        <h2>История драфтов</h2>
        <span className="muted">Последние 100</span>
      </div>
      {drafts.length === 0 ? (
        <p className="muted">Изменений пока нет.</p>
      ) : (
        <table>
          <thead>
            <tr>
              <th>Драфт</th>
              <th>Изменение</th>
              <th>Статус</th>
              <th>Версия</th>
            </tr>
          </thead>
          <tbody>
            {drafts.map((item) => (
              <tr key={item.id}>
                <td>
                  <button disabled={busy || editor} onClick={() => setDraft(item)}>
                    #{item.id}
                  </button>
                </td>
                <td>{item.description}</td>
                <td>{statusName[item.status]}</td>
                <td>{item.version}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </section>
  );
}
