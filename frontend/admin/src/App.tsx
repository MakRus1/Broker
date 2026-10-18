import { useWorkspace } from './hooks/useWorkspace';
import { LoginScreen } from './components/LoginScreen';
import { Catalog } from './components/Catalog';
import { ConfigDrawer } from './components/ConfigDrawer';
import { Notifications } from './components/Notifications';

export function App() {
  const workspace = useWorkspace();
  const { session, selected, catalog, busy, run, logout } = workspace;
  if (!session) return <LoginScreen workspace={workspace} />;
  return (
    <main className="workspace">
      <header>
        <div className="brand">
          B<span>Broker</span>
        </div>
        <div className="account">
          <span title={session.actor}>{session.display_name ?? session.subject}</span>
          <button disabled={busy} onClick={() => run(logout)}>
            Выйти
          </button>
        </div>
      </header>
      <div className="heading">
        <div>
          <p className="eyebrow">АДМИНИСТРИРОВАНИЕ</p>
          <h1>
            Конфиги <span className="count">{catalog.length}</span>
          </h1>
          <p>Изменения под контролем. Значения, согласование и история — в одном месте.</p>
        </div>
      </div>
      <Catalog workspace={workspace} />
      {selected && <ConfigDrawer key={selected} workspace={workspace} />}
      <Notifications workspace={workspace} />
    </main>
  );
}
