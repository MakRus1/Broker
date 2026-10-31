import type { Draft } from '../../shared/api/api';
import type { Workspace } from '../../app/useWorkspace';
import { statusName } from './presentation';

export function DraftHistory({
  workspace,
  onSelect,
}: {
  workspace: Workspace;
  onSelect: (draft: Draft) => void;
}) {
  const { drafts, busy, editor } = workspace;
  return (
    <section className="panel">
      <div className="panelhead">
        <h2>История изменений</h2>
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
              <tr
                key={item.id}
                className="history-row"
                aria-disabled={busy || editor}
                onClick={() => {
                  if (!busy && !editor) onSelect(item);
                }}
              >
                <td>
                  <button
                    disabled={busy || editor}
                    onClick={(event) => {
                      event.stopPropagation();
                      onSelect(item);
                    }}
                  >
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
