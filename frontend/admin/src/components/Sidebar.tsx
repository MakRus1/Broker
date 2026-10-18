import type { Workspace } from '../hooks/useWorkspace';

export function Sidebar({ workspace }: { workspace: Workspace }) {
  const { catalog, selected, busy, editor, session, setSelected, run, logout } = workspace;
  if (!session) return null;
  return (
    <aside>
      <div className="brand">
        B<span>Broker</span>
      </div>
      <p className="eyebrow">АДМИНИСТРИРОВАНИЕ</p>
      <h2>
        Конфиги <span className="count">{catalog.length}</span>
      </h2>
      <nav aria-label="Конфиги">
        {catalog.map((item) => (
          <button
            className={'navitem ' + (selected === item.config_name ? 'selected' : '')}
            key={item.config_name}
            disabled={busy || editor}
            onClick={() => setSelected(item.config_name)}
          >
            <span>{item.config_name}</span>
            <small>
              Версия {item.version}
              {item.active_draft_id ? ' · Есть драфт' : ''}
            </small>
          </button>
        ))}
      </nav>
      <footer>
        <span className="avatar">●</span>
        <span title={session.actor}>{session.display_name ?? session.subject}</span>
        <button disabled={busy} onClick={() => run(logout)}>
          Выйти
        </button>
      </footer>
    </aside>
  );
}
