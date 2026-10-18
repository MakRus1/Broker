import type { Workspace } from '../hooks/useWorkspace';
import { ValueDiff } from './ValueDiff';

export function ConfigEditor({ workspace }: { workspace: Workspace }) {
  const {
    description,
    setDescription,
    text,
    setText,
    busy,
    run,
    save,
    setEditor,
    editing,
    saveDisabled,
    current,
  } = workspace;
  return (
    <form
      onSubmit={(event) => {
        event.preventDefault();
        void run(save);
      }}
    >
      <label htmlFor="value">Значение · JSON</label>
      <textarea
        id="value"
        spellCheck={false}
        rows={12}
        value={text}
        onChange={(event) => setText(event.target.value)}
        disabled={busy}
        aria-invalid={!!editing.error}
        aria-describedby="value-help"
      />
      <p id="value-help" className={editing.error ? 'field-error' : 'muted'}>
        {editing.error ||
          (!editing.changed
            ? 'Измените значение, чтобы сохранить драфт.'
            : 'Изменения будут отправлены на подтверждение.')}
      </p>
      <label htmlFor="description">
        Что меняется и зачем <strong>· обязательно</strong>
      </label>
      <input
        id="description"
        value={description}
        maxLength={4096}
        required
        aria-describedby="description-help"
        onChange={(event) => setDescription(event.target.value)}
        disabled={busy}
        placeholder="Например: увеличиваем срок доставки для морских перевозок"
      />
      {!description.trim() && (
        <p id="description-help" className="field-error">
          Укажите причину изменения.
        </p>
      )}
      <div className="actions">
        <button type="submit" className="primary" disabled={saveDisabled}>
          {busy ? 'Сохраняем…' : 'Готово'}
        </button>
        <button type="button" disabled={busy} onClick={() => setEditor(false)}>
          Отменить редактирование
        </button>
      </div>
      {!editing.error && <ValueDiff before={current?.value} after={JSON.parse(text)} />}
    </form>
  );
}
