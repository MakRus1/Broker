import { lazy, Suspense } from 'react';
import type { Workspace } from '../hooks/useWorkspace';
import { ValueDiff } from './ValueDiff';

const JsonEditor = lazy(() =>
  import('./JsonEditor').then((module) => ({ default: module.JsonEditor })),
);

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
    discardChanges,
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
      {workspace.creating && (
        <>
          <label htmlFor="config-name">Название конфига · обязательно</label>
          <input
            id="config-name"
            value={workspace.newName}
            maxLength={256}
            required
            pattern="[A-Z][A-Z0-9]*(_[A-Z0-9]+)*"
            disabled={busy}
            onChange={(event) => workspace.setNewName(event.target.value)}
          />
          <p className="muted">
            Латинские заглавные буквы, цифры и подчёркивания, например DELIVERY_SETTINGS.
          </p>
        </>
      )}
      {workspace.removing ? (
        <p>
          После подтверждения конфиг исчезнет из выдачи сервисам. История сохранится. Обязательный
          конфиг может повлиять на готовность сервиса.
        </p>
      ) : (
        <>
          <label htmlFor="value">Значение · JSON</label>
          <Suspense fallback={<p>Загружаем редактор…</p>}>
            <JsonEditor value={text} onChange={setText} disabled={busy} invalid={!!editing.error} />
          </Suspense>
          <p id="value-help" className={editing.error ? 'field-error' : 'muted'}>
            {editing.error ||
              (!workspace.creating && !editing.changed
                ? 'Измените значение, чтобы сохранить драфт.'
                : 'Изменения будут отправлены на подтверждение.')}
          </p>
        </>
      )}
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
          {busy ? 'Сохраняем…' : workspace.removing ? 'Создать драфт удаления' : 'Готово'}
        </button>
        <button
          type="button"
          disabled={busy}
          onClick={() => {
            discardChanges(() => (workspace.creating ? workspace.closeConfig() : setEditor(false)));
          }}
        >
          Отменить редактирование
        </button>
      </div>
      {!editing.error && (
        <ValueDiff before={current?.value} after={workspace.removing ? null : JSON.parse(text)} />
      )}
    </form>
  );
}
