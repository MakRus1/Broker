import { PermissionGroups } from './PermissionGroups';
import { useState } from 'react';
import { EditorPanel } from './EditorPanel';
import type { OrganizationRole, Permission } from './types';

export function RoleEditor({
  role,
  permissions,
  busy,
  error,
  save,
  close,
}: {
  role?: OrganizationRole;
  permissions: Permission[];
  busy: boolean;
  error: string;
  save: (value: Pick<OrganizationRole, 'name' | 'description' | 'permissions'>) => Promise<void>;
  close: () => void;
}) {
  const [name, setName] = useState(role?.name ?? '');
  const [description, setDescription] = useState(role?.description ?? '');
  const [selected, setSelected] = useState(role?.permissions ?? []);
  const dirty =
    name !== (role?.name ?? '') ||
    description !== (role?.description ?? '') ||
    [...selected].sort().join() !== [...(role?.permissions ?? [])].sort().join();
  return (
    <EditorPanel
      title={role ? `Роль «${role.name}»` : 'Новая роль'}
      dirty={dirty}
      busy={busy}
      close={close}
    >
      <form
        className="organization-form"
        onSubmit={(event) => {
          event.preventDefault();
          void save({ name: name.trim(), description: description.trim(), permissions: selected });
        }}
      >
        {error && (
          <div role="alert" className="organization-error">
            {error}
          </div>
        )}
        <label>
          Название роли
          <input
            value={name}
            required
            maxLength={100}
            disabled={busy}
            onChange={(event) => setName(event.target.value)}
          />
        </label>
        <label>
          Описание
          <textarea
            value={description}
            maxLength={500}
            rows={3}
            disabled={busy}
            onChange={(event) => setDescription(event.target.value)}
          />
        </label>
        <PermissionGroups
          permissions={permissions}
          selected={selected}
          busy={busy}
          onChange={setSelected}
        />
        <div className="organization-editor-actions">
          <button className="primary" disabled={busy || !name.trim() || !dirty}>
            {busy ? 'Сохраняем…' : role ? 'Сохранить роль' : 'Создать роль'}
          </button>
        </div>
      </form>
    </EditorPanel>
  );
}
