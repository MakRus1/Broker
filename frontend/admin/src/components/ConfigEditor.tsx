import type { Workspace } from '../hooks/useWorkspace';

export function ConfigEditor({ workspace }: { workspace: Workspace }) {
  const { owner, description, setDescription, text, setText, busy, run, save, setEditor } =
    workspace;
  return (
    <section className="panel">
      <div className="panelhead">
        <h2>{owner ? 'Редактирование драфта' : 'Новое изменение'}</h2>
        <span className="muted">Публикация после подтверждения</span>
      </div>
      <label htmlFor="description">Причина изменения</label>
      <input
        id="description"
        value={description}
        maxLength={4096}
        onChange={(event) => setDescription(event.target.value)}
        disabled={busy}
        placeholder="Что меняется и зачем"
      />
      <label htmlFor="value">Значение · JSON</label>
      <textarea
        id="value"
        spellCheck={false}
        rows={18}
        value={text}
        onChange={(event) => setText(event.target.value)}
        disabled={busy}
      />
      <div className="actions">
        <button className="primary" disabled={busy} onClick={() => run(save)}>
          {busy ? 'Сохраняем…' : 'Готово'}
        </button>
        <button disabled={busy} onClick={() => setEditor(false)}>
          Отменить редактирование
        </button>
      </div>
      <p className="muted">
        Невалидный драфт не сохранится. Окончательную проверку выполняет Configs.
      </p>
    </section>
  );
}
