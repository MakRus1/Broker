import { useWorkspace } from './hooks/useWorkspace';
import { LoginScreen } from './components/LoginScreen';
import { Sidebar } from './components/Sidebar';
import { CurrentValue } from './components/CurrentValue';
import { ConfigEditor } from './components/ConfigEditor';
import { DraftDetails } from './components/DraftDetails';
import { DraftHistory } from './components/DraftHistory';

export function App() {
  const workspace = useWorkspace();
  const { session, selected, current, error, notice, busy, editor, draft, run, reload } = workspace;

  if (!session) return <LoginScreen workspace={workspace} />;

  return (
    <div className="shell">
      <Sidebar workspace={workspace} />
      <main>
        <header>
          <span>Платформа / Конфиги</span>
          <span className="badge">Согласование изменений</span>
        </header>
        <div className="heading">
          <div>
            <p className="eyebrow">ДИНАМИЧЕСКАЯ КОНФИГУРАЦИЯ</p>
            <h1>{selected || 'Выберите конфиг'}</h1>
            {current && (
              <p>
                Рабочая версия {current.version} · Изменения применяются без перезапуска сервисов
              </p>
            )}
          </div>
          {selected && (
            <button disabled={busy || editor} onClick={() => run(reload)}>
              Обновить
            </button>
          )}
        </div>
        {error && (
          <div role="alert" className="error">
            {error}
          </div>
        )}
        {notice && (
          <div role="status" className="notice">
            {notice}
          </div>
        )}
        {!selected && (
          <section className="empty">
            <span>◈</span>
            <h2>У каждого изменения — история</h2>
            <p>Откройте конфиг слева, чтобы посмотреть рабочее значение или подготовить драфт.</p>
          </section>
        )}
        {selected && !current && <p>Загружаем конфиг…</p>}
        {current && (
          <>
            <CurrentValue workspace={workspace} />
            {editor && <ConfigEditor workspace={workspace} />}
            {draft && !editor && <DraftDetails workspace={workspace} />}
            <DraftHistory workspace={workspace} />
          </>
        )}
      </main>
    </div>
  );
}
