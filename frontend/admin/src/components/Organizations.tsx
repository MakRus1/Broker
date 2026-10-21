import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request, type Session } from '../api';

export type Organization = { id: string; name: string; enabled: boolean; revision: number };
export type MemberRole = 'administrator' | 'operator' | 'viewer';
export type Member = {
  subject: string;
  username: string;
  display_name: string;
  email: string;
  role: MemberRole;
  enabled: boolean;
  revision: number;
};
export type OrganizationAccess = {
  platform_admin: boolean;
  organizations: Organization[];
  managed_organization_ids: string[];
};
const roles: Record<MemberRole, string> = {
  administrator: 'Администратор организации',
  operator: 'Сотрудник — чтение и изменение',
  viewer: 'Сотрудник — только чтение',
};

function RoleOptions() {
  return Object.entries(roles).map(([value, title]) => (
    <option key={value} value={value}>
      {title}
    </option>
  ));
}

function MemberRow({
  member,
  save,
  busy,
}: {
  member: Member;
  save: (member: Member, role: MemberRole, enabled: boolean) => Promise<void>;
  busy: boolean;
}) {
  const [role, setRole] = useState(member.role);
  const [enabled, setEnabled] = useState(member.enabled);
  const changed = role !== member.role || enabled !== member.enabled;
  return (
    <tr>
      <td>
        <strong>{member.display_name}</strong>
        <small>
          {member.username} · {member.email}
        </small>
      </td>
      <td>
        <select
          aria-label={`Роль: ${member.username}`}
          value={role}
          disabled={busy}
          onChange={(event) => setRole(event.target.value as MemberRole)}
        >
          <RoleOptions />
        </select>
      </td>
      <td>
        <label className="membership-status">
          <input
            type="checkbox"
            checked={enabled}
            disabled={busy}
            onChange={(event) => setEnabled(event.target.checked)}
          />
          Доступ разрешён
        </label>
      </td>
      <td>
        <button disabled={busy || !changed} onClick={() => void save(member, role, enabled)}>
          Сохранить
        </button>
      </td>
    </tr>
  );
}

export function Organizations({
  session,
  access,
}: {
  session: Session;
  access: OrganizationAccess;
}) {
  const [organizations, setOrganizations] = useState(access.organizations);
  const [selectedId, setSelectedId] = useState('');
  const selected = organizations.find((item) => item.id === selectedId);
  const [members, setMembers] = useState<Member[]>([]);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  const [organizationName, setOrganizationName] = useState('');
  const [mode, setMode] = useState<'create' | 'existing'>('create');
  const pendingCreate = useRef<{ key: string; id: string } | null>(null);
  const [canLoadOrganizations, setCanLoadOrganizations] = useState(false);
  const [canLoadMembers, setCanLoadMembers] = useState(false);

  async function run(action: () => Promise<void>) {
    if (busy) return;
    setBusy(true);
    setError('');
    setNotice('');
    try {
      await action();
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Не удалось выполнить действие');
    } finally {
      setBusy(false);
    }
  }
  async function loadOrganizations(append = false) {
    const result = await request<{ items: Organization[] }>(
      '/api/admin/organizations/list',
      session,
      { limit: 50, ...(append && organizations.length ? { after: organizations.at(-1)?.id } : {}) },
    );
    setOrganizations((current) => (append ? [...current, ...result.items] : result.items));
    setCanLoadOrganizations(result.items.length === 50);
  }
  async function loadMembers(organizationId: string, append = false) {
    const result = await request<{ items: Member[] }>(
      '/api/admin/organizations/members/list',
      session,
      {
        organization_id: organizationId,
        limit: 50,
        ...(append && members.length ? { after: members.at(-1)?.subject } : {}),
      },
    );
    setMembers((current) => (append ? [...current, ...result.items] : result.items));
    setCanLoadMembers(result.items.length === 50);
  }
  useEffect(() => {
    let active = true;
    if (access.platform_admin) {
      request<{ items: Organization[] }>('/api/admin/organizations/list', session, { limit: 50 })
        .then((value) => {
          if (active) {
            setOrganizations(value.items);
            setCanLoadOrganizations(value.items.length === 50);
          }
        })
        .catch((reason: unknown) => {
          if (active)
            setError(reason instanceof Error ? reason.message : 'Не удалось загрузить организации');
        });
    }
    return () => {
      active = false;
    };
  }, [session, access.platform_admin]);
  useEffect(() => {
    setMembers([]);
    setError('');
    setNotice('');
    if (!selectedId) return;
    let active = true;
    request<{ items: Member[] }>('/api/admin/organizations/members/list', session, {
      organization_id: selectedId,
      limit: 50,
    })
      .then((value) => {
        if (active) {
          setMembers(value.items);
          setCanLoadMembers(value.items.length === 50);
        }
      })
      .catch((reason: unknown) => {
        if (active)
          setError(reason instanceof Error ? reason.message : 'Не удалось загрузить сотрудников');
      });
    return () => {
      active = false;
    };
  }, [session, selectedId]);

  function createOrganization(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    void run(async () => {
      const { organization } = await request<{ organization: Organization }>(
        '/api/admin/organizations/create',
        session,
        { id: crypto.randomUUID(), name: organizationName.trim() },
      );
      setOrganizationName('');
      await loadOrganizations();
      setOrganizations((current) =>
        current.some((item) => item.id === organization.id) ? current : [...current, organization],
      );
      setSelectedId(organization.id);
      setNotice('Организация создана. Добавьте первого администратора.');
    });
  }
  function createMember(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const form = event.currentTarget;
    const data = new FormData(form);
    void run(async () => {
      const common = {
        organization_id: selectedId,
        username: String(data.get('username')).trim(),
        role: data.get('role'),
      };
      const body =
        mode === 'existing'
          ? common
          : {
              ...common,
              request_id: '',
              email: String(data.get('email')).trim(),
              display_name: String(data.get('display_name')).trim(),
              temporary_password: String(data.get('temporary_password')),
            };
      if ('request_id' in body) {
        const key = JSON.stringify({
          ...body,
          request_id: undefined,
          temporary_password: undefined,
        });
        if (pendingCreate.current?.key !== key)
          pendingCreate.current = { key, id: crypto.randomUUID() };
        body.request_id = pendingCreate.current.id;
      }
      await request(
        `/api/admin/organizations/members/${mode === 'existing' ? 'add' : 'create'}`,
        session,
        body,
      );
      form.reset();
      pendingCreate.current = null;
      await loadMembers(selectedId);
      setNotice(
        mode === 'create'
          ? 'Сотрудник создан. При первом входе он должен сменить временный пароль.'
          : 'Пользователь добавлен в организацию.',
      );
    });
  }
  async function saveMember(member: Member, role: MemberRole, enabled: boolean) {
    await run(async () => {
      await request('/api/admin/organizations/members/update', session, {
        organization_id: selectedId,
        subject: member.subject,
        expected_revision: member.revision,
        role,
        enabled,
      });
      await loadMembers(selectedId);
      setNotice('Доступ сотрудника обновлён.');
    });
  }
  return (
    <section className="organizations-workspace">
      <div className="heading">
        <div>
          <p className="eyebrow">АДМИНИСТРИРОВАНИЕ</p>
          <h1>Организации и сотрудники</h1>
          <p>
            {access.platform_admin
              ? 'Управление всеми организациями платформы.'
              : 'Управление сотрудниками ваших организаций.'}
          </p>
        </div>
      </div>
      {error && (
        <div role="alert" className="organization-error">
          {error}
        </div>
      )}
      {notice && (
        <div role="status" className="organization-notice">
          {notice}
        </div>
      )}
      <div className="organization-columns">
        <aside className="organization-list">
          <h2>Организации</h2>
          {access.platform_admin && (
            <form onSubmit={createOrganization} className="organization-form">
              <label>
                Название новой организации
                <input
                  value={organizationName}
                  maxLength={200}
                  required
                  onChange={(event) => setOrganizationName(event.target.value)}
                />
              </label>
              <button disabled={busy || !organizationName.trim()}>Создать организацию</button>
            </form>
          )}
          {organizations
            .filter(
              (item) => access.platform_admin || access.managed_organization_ids.includes(item.id),
            )
            .map((organization) => (
              <button
                key={organization.id}
                className={`organization-choice ${selectedId === organization.id ? 'selected' : ''}`}
                disabled={busy}
                onClick={() => setSelectedId(organization.id)}
              >
                {organization.name}
                <small>{organization.enabled ? 'Активна' : 'Отключена'}</small>
              </button>
            ))}
          {canLoadOrganizations && (
            <button disabled={busy} onClick={() => void run(() => loadOrganizations(true))}>
              Показать ещё
            </button>
          )}
        </aside>
        <section className="organization-members">
          {!selected ? (
            <p>Выберите организацию, чтобы управлять сотрудниками.</p>
          ) : (
            <>
              <div className="organization-title">
                <h2>{selected.name}</h2>
                <button disabled={busy} onClick={() => void run(() => loadMembers(selectedId))}>
                  Обновить сотрудников
                </button>
              </div>
              {access.platform_admin && (
                <button
                  disabled={busy}
                  onClick={() =>
                    void run(async () => {
                      await request('/api/admin/organizations/update', session, {
                        id: selected.id,
                        name: selected.name,
                        enabled: !selected.enabled,
                        expected_revision: selected.revision,
                      });
                      await loadOrganizations();
                    })
                  }
                >
                  {selected.enabled ? 'Отключить доступ к организации' : 'Включить организацию'}
                </button>
              )}
              <div className="members-table">
                <table>
                  <thead>
                    <tr>
                      <th>Сотрудник</th>
                      <th>Роль</th>
                      <th>Статус</th>
                      <th>Действие</th>
                    </tr>
                  </thead>
                  <tbody>
                    {members.map((member) => (
                      <MemberRow
                        key={`${selectedId}:${member.subject}:${member.revision}`}
                        member={member}
                        busy={busy}
                        save={saveMember}
                      />
                    ))}
                  </tbody>
                </table>
              </div>
              {!members.length && (
                <p>Сотрудников пока нет. Начните с администратора организации.</p>
              )}
              {canLoadMembers && (
                <button
                  disabled={busy}
                  onClick={() => void run(() => loadMembers(selectedId, true))}
                >
                  Показать ещё сотрудников
                </button>
              )}
              <h3>Добавить сотрудника</h3>
              <div className="organization-modes">
                <button
                  aria-pressed={mode === 'create'}
                  onClick={() => {
                    setMode('create');
                    pendingCreate.current = null;
                  }}
                >
                  Новая учётная запись
                </button>
                <button aria-pressed={mode === 'existing'} onClick={() => setMode('existing')}>
                  Существующий пользователь
                </button>
              </div>
              <form
                key={`${mode}:${selectedId}`}
                className="organization-form member-form"
                onSubmit={createMember}
              >
                <label>
                  Логин
                  <input name="username" required maxLength={100} autoComplete="off" />
                </label>
                {mode === 'create' && (
                  <>
                    <label>
                      Имя сотрудника
                      <input name="display_name" required maxLength={200} />
                    </label>
                    <label>
                      Email
                      <input
                        name="email"
                        type="email"
                        required
                        maxLength={254}
                        autoComplete="off"
                      />
                    </label>
                    <label>
                      Временный пароль · минимум 12 символов
                      <input
                        name="temporary_password"
                        type="password"
                        required
                        minLength={12}
                        maxLength={256}
                        autoComplete="new-password"
                      />
                    </label>
                  </>
                )}
                <label>
                  Роль
                  <select name="role" defaultValue="operator">
                    <RoleOptions />
                  </select>
                </label>
                <button disabled={busy}>
                  {busy
                    ? 'Сохраняем…'
                    : mode === 'create'
                      ? 'Создать сотрудника'
                      : 'Добавить в организацию'}
                </button>
              </form>
              <p className="organization-help">
                Отключение сотрудника запрещает доступ только к этой организации. Учётная запись и
                доступ к другим организациям сохраняются.
              </p>
            </>
          )}
        </section>
      </div>
    </section>
  );
}
