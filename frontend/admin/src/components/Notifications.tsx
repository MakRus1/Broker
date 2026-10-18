import type { Workspace } from '../hooks/useWorkspace';

export function Notifications({ workspace }: { workspace: Workspace }) {
  const { error, notice, setError, setNotice } = workspace;
  return (
    <div className="notifications">
      {error && (
        <div role="alert" className="error">
          <strong>Не удалось выполнить действие</strong>
          <p>{error}</p>
          <button aria-label="Закрыть ошибку" onClick={() => setError('')}>
            ✕
          </button>
        </div>
      )}
      {notice && (
        <div role="status" className="notice">
          <p>{notice}</p>
          <button aria-label="Закрыть уведомление" onClick={() => setNotice('')}>
            ✕
          </button>
        </div>
      )}
    </div>
  );
}
