import type { Workspace } from './useWorkspace';

export function LoginScreen({ workspace }: { workspace: Workspace }) {
  const { loginRequired, error } = workspace;
  return (
    <main className="welcome">
      <div className="brand">
        B<span>Broker</span>
      </div>
      <p className="eyebrow">УПРАВЛЕНИЕ ПЛАТФОРМОЙ</p>
      <h1>
        Платформа
        <br />
        под контролем.
      </h1>
      <p>Управляйте организациями, сотрудниками и конфигурацией Broker.</p>
      {loginRequired ? (
        <a className="primary link" href="/auth/login">
          Войти →
        </a>
      ) : (
        <p>Подключаемся…</p>
      )}
      {error && !loginRequired && (
        <p role="alert" className="error">
          {error} <button onClick={() => location.reload()}>Повторить</button>
        </p>
      )}
    </main>
  );
}
