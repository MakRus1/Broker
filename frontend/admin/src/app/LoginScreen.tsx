import type { Workspace } from './useWorkspace';

import { initialThemePreference } from '../../../shared/theme/theme';

export function LoginScreen({ workspace }: { workspace: Workspace }) {
  const { loginRequired, error } = workspace;
  return (
    <main className="welcome">
      <div className="brand">
        B<span>Broker</span>
      </div>
      <h1>Администрирование</h1>
      <p>Организации, сотрудники, настройки и переводы.</p>
      {loginRequired ? (
        <a className="primary link" href={`/auth/login?theme=${initialThemePreference()}`}>
          Войти
        </a>
      ) : (
        <p>Загрузка…</p>
      )}
      {error && !loginRequired && (
        <p role="alert" className="error">
          {error} <button onClick={() => location.reload()}>Повторить</button>
        </p>
      )}
    </main>
  );
}
