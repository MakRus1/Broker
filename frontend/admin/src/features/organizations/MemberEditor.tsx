import { useState } from 'react';
import { EditorPanel } from './EditorPanel';
import type { Member, MemberInput, OrganizationRole } from './types';

export function MemberEditor({
  member,
  roles,
  busy,
  error,
  save,
  close,
  platformAccess,
}: {
  member?: Member;
  roles: OrganizationRole[];
  busy: boolean;
  error: string;
  save: (input: MemberInput) => Promise<void>;
  close: () => void;
  platformAccess?: () => void;
}) {
  const initial: MemberInput = {
    mode: member ? 'edit' : 'create',
    username: member?.username ?? '',
    last_name: member?.last_name ?? '',
    first_name: member?.first_name ?? '',
    middle_name: member?.middle_name ?? '',
    email: member?.email ?? '',
    temporary_password: '',
    role:
      member?.role ??
      (roles.some((role) => role.id === 'operator') ? 'operator' : (roles[0]?.id ?? '')),
    enabled: member?.enabled ?? true,
  };
  const [value, setValue] = useState(initial);
  const profileChanged =
    value.last_name !== initial.last_name ||
    value.first_name !== initial.first_name ||
    value.middle_name !== initial.middle_name;
  const dirty = JSON.stringify(value) !== JSON.stringify(initial);
  const selectedRole = roles.find((role) => role.id === value.role);
  const change = <Key extends keyof MemberInput>(key: Key, next: MemberInput[Key]) =>
    setValue((current) => ({ ...current, [key]: next }));
  return (
    <EditorPanel
      title={member ? member.display_name : 'Добавить сотрудника'}
      dirty={dirty}
      busy={busy}
      close={close}
    >
      <form
        className="organization-form"
        onSubmit={(event) => {
          event.preventDefault();
          void save(value);
        }}
      >
        {error && (
          <div role="alert" className="organization-error">
            {error}
          </div>
        )}
        {!member ? (
          <>
            <fieldset disabled={busy} className="organization-mode-select">
              <legend>Учётная запись</legend>
              <label>
                <input
                  type="radio"
                  name="account-mode"
                  checked={value.mode === 'create'}
                  onChange={() => change('mode', 'create')}
                />
                Создать новую
              </label>
              <label>
                <input
                  type="radio"
                  name="account-mode"
                  checked={value.mode === 'existing'}
                  onChange={() => change('mode', 'existing')}
                />
                Добавить существующую
              </label>
            </fieldset>
            <label>
              Логин
              <input
                value={value.username}
                required
                maxLength={100}
                autoComplete="off"
                disabled={busy}
                onChange={(event) => change('username', event.target.value)}
              />
            </label>
            {value.mode === 'create' && (
              <>
                {(
                  [
                    ['last_name', 'Фамилия'],
                    ['first_name', 'Имя'],
                    ['middle_name', 'Отчество'],
                  ] as const
                ).map(([key, label]) => (
                  <label key={key}>
                    {label}
                    {key === 'middle_name' ? ' · необязательно' : ''}
                    <input
                      value={value[key]}
                      required={key !== 'middle_name'}
                      maxLength={100}
                      autoComplete={
                        key === 'last_name'
                          ? 'family-name'
                          : key === 'first_name'
                            ? 'given-name'
                            : 'additional-name'
                      }
                      disabled={busy}
                      onChange={(event) => change(key, event.target.value)}
                    />
                  </label>
                ))}
                <label>
                  Email
                  <input
                    value={value.email}
                    type="email"
                    required
                    maxLength={254}
                    autoComplete="off"
                    disabled={busy}
                    onChange={(event) => change('email', event.target.value)}
                  />
                </label>
                <label>
                  Временный пароль
                  <input
                    value={value.temporary_password}
                    type="password"
                    required
                    minLength={12}
                    maxLength={256}
                    autoComplete="new-password"
                    disabled={busy}
                    onChange={(event) => change('temporary_password', event.target.value)}
                  />
                  <small>Минимум 12 символов. При первом входе сотрудник задаст свой пароль.</small>
                </label>
              </>
            )}
          </>
        ) : (
          <dl className="member-summary">
            {(
              [
                ['Фамилия', member.last_name],
                ['Имя', member.first_name],
                ['Отчество', member.middle_name],
              ] as const
            ).map(([label, name]) => (
              <div key={label}>
                <dt>{label}</dt>
                <dd>{name || 'Не указано'}</dd>
              </div>
            ))}
            <div>
              <dt>Логин</dt>
              <dd>{member.username}</dd>
            </div>
            <div>
              <dt>Email</dt>
              <dd>{member.email}</dd>
            </div>
          </dl>
        )}
        {member && (
          <fieldset disabled={busy}>
            <legend>ФИО</legend>
            {(
              [
                ['last_name', 'Фамилия'],
                ['first_name', 'Имя'],
                ['middle_name', 'Отчество'],
              ] as const
            ).map(([key, label]) => (
              <label key={key}>
                {label}
                {key === 'middle_name' ? ' · необязательно' : ''}
                <input
                  value={value[key]}
                  required={profileChanged && key !== 'middle_name'}
                  maxLength={100}
                  onChange={(event) => change(key, event.target.value)}
                />
              </label>
            ))}
            <small>
              Профиль общий для всех организаций. Изменения имени при входе появятся после повторной
              авторизации.
            </small>
          </fieldset>
        )}
        <label>
          Роль
          <select
            value={value.role}
            required
            disabled={busy}
            onChange={(event) => change('role', event.target.value)}
          >
            {roles.map((role) => (
              <option key={role.id} value={role.id}>
                {role.name}
              </option>
            ))}
          </select>
        </label>
        {selectedRole && (
          <div className="role-preview">
            <strong>{selectedRole.name}</strong>
            <p>{selectedRole.description}</p>
          </div>
        )}
        {member && (
          <label className="member-access">
            <input
              type="checkbox"
              checked={value.enabled}
              disabled={busy}
              onChange={(event) => change('enabled', event.target.checked)}
            />
            Доступ к организации
          </label>
        )}
        {platformAccess && (
          <section className="role-preview">
            <h3>Права платформы</h3>
            <button type="button" disabled={dirty || busy} onClick={platformAccess}>
              Настроить доступ
            </button>
          </section>
        )}
        <div className="organization-editor-actions">
          <button
            className="primary"
            disabled={busy || !value.role || (member !== undefined && !dirty)}
          >
            {busy ? 'Сохраняем…' : member ? 'Сохранить изменения' : 'Добавить сотрудника'}
          </button>
        </div>
      </form>
    </EditorPanel>
  );
}
