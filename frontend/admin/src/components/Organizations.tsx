import { useEffect, useRef, useState } from 'react';
import { request, type Session } from '../api';
import { EditorPanel } from './organizations/EditorPanel';
import { MemberEditor } from './organizations/MemberEditor';
import { RoleEditor } from './organizations/RoleEditor';
import type {
  Member,
  MemberInput,
  Organization,
  OrganizationAccess,
  OrganizationRole,
  Permission,
} from './organizations/types';
export type { OrganizationAccess } from './organizations/types';

type Editor =
  | { kind: 'member'; member?: Member }
  | { kind: 'role'; role?: OrganizationRole }
  | { kind: 'organization'; organization?: Organization };

export function Organizations({
  session,
  access,
}: {
  session: Session;
  access: OrganizationAccess;
}) {
  const ownOrganizations = access.organizations.filter((item) =>
    access.managed_organization_ids.includes(item.id),
  );
  const [organizations, setOrganizations] = useState(access.platform_admin ? [] : ownOrganizations);
  const [selected, setSelected] = useState<Organization | undefined>(
    access.platform_admin ? undefined : ownOrganizations[0],
  );
  const [tab, setTab] = useState<'members' | 'roles'>('members');
  const [members, setMembers] = useState<Member[]>([]);
  const [roles, setRoles] = useState<OrganizationRole[]>([]);
  const [permissions, setPermissions] = useState<Permission[]>([]);
  const [editor, setEditor] = useState<Editor>();
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  const [hasMore, setHasMore] = useState(false);
  const [moreMembers, setMoreMembers] = useState(false);
  const newRoleId = useRef('');
  const pendingCreate = useRef<{ key: string; id: string } | null>(null);

  function openOrganization(organization: Organization | undefined) {
    const hash = organization ? '#/organizations/' + organization.id : '';
    if (window.location.hash !== hash) window.location.hash = hash;
    setSelected(organization);
    setTab('members');
    setError('');
  }
  useEffect(() => {
    function restoreOrganization() {
      const prefix = '#/organizations/';
      const id = window.location.hash.startsWith(prefix)
        ? window.location.hash.slice(prefix.length)
        : undefined;
      const available = access.platform_admin ? organizations : ownOrganizations;
      setSelected(
        id
          ? available.find((item) => item.id === id)
          : access.platform_admin
            ? undefined
            : available[0],
      );
      setTab('members');
      setError('');
    }
    restoreOrganization();
    window.addEventListener('hashchange', restoreOrganization);
    return () => window.removeEventListener('hashchange', restoreOrganization);
  }, [organizations, access]);

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
    const value = await request<{ items: Organization[] }>(
      '/api/admin/organizations/list',
      session,
      { limit: 50, ...(append && organizations.length ? { after: organizations.at(-1)?.id } : {}) },
    );
    setOrganizations((current) => (append ? [...current, ...value.items] : value.items));
    setHasMore(value.items.length === 50);
  }
  async function loadMembers(organization: Organization, append = false) {
    const value = await request<{ items: Member[] }>(
      '/api/admin/organizations/members/list',
      session,
      {
        organization_id: organization.id,
        limit: 50,
        ...(append && members.length ? { after: members.at(-1)?.subject } : {}),
      },
    );
    setMembers((current) => (append ? [...current, ...value.items] : value.items));
    setMoreMembers(value.items.length === 50);
  }
  async function loadRoles(organization: Organization) {
    const value = await request<{ items: OrganizationRole[]; permissions: Permission[] }>(
      '/api/admin/organizations/roles/list',
      session,
      { organization_id: organization.id },
    );
    setRoles(value.items);
    setPermissions(value.permissions);
  }
  useEffect(() => {
    if (!access.platform_admin) return;
    let active = true;
    request<{ items: Organization[] }>('/api/admin/organizations/list', session, { limit: 50 })
      .then((value) => {
        if (active) {
          setOrganizations(value.items);
          setHasMore(value.items.length === 50);
        }
      })
      .catch((reason: unknown) => {
        if (active)
          setError(reason instanceof Error ? reason.message : 'Не удалось загрузить организации');
      });
    return () => {
      active = false;
    };
  }, [session, access.platform_admin]);
  useEffect(() => {
    if (!selected) return;
    let active = true;
    setLoading(true);
    setMembers([]);
    setRoles([]);
    setError('');
    Promise.all([
      request<{ items: Member[] }>('/api/admin/organizations/members/list', session, {
        organization_id: selected.id,
        limit: 50,
      }),
      request<{ items: OrganizationRole[]; permissions: Permission[] }>(
        '/api/admin/organizations/roles/list',
        session,
        { organization_id: selected.id },
      ),
    ])
      .then(([people, roleData]) => {
        if (!active) return;
        setMembers(people.items);
        setMoreMembers(people.items.length === 50);
        setRoles(roleData.items);
        setPermissions(roleData.permissions);
      })
      .catch((reason: unknown) => {
        if (active)
          setError(reason instanceof Error ? reason.message : 'Не удалось загрузить организацию');
      })
      .finally(() => {
        if (active) setLoading(false);
      });
    return () => {
      active = false;
    };
  }, [session, selected?.id]);

  function openEditor(value: Editor) {
    setError('');
    setNotice('');
    pendingCreate.current = null;
    if (value.kind === 'role' && !value.role)
      newRoleId.current = 'role_' + crypto.randomUUID().replaceAll('-', '');
    setEditor(value);
  }
  function closeEditor() {
    setEditor(undefined);
    setError('');
    pendingCreate.current = null;
  }
  async function saveMember(value: MemberInput) {
    if (!selected || editor?.kind !== 'member') return;
    const organization = selected;
    const member = editor.member;
    await run(async () => {
      if (value.mode === 'edit' && member) {
        await request('/api/admin/organizations/members/update', session, {
          organization_id: organization.id,
          subject: member.subject,
          expected_revision: member.revision,
          role: value.role,
          enabled: value.enabled,
        });
      } else {
        const common = {
          organization_id: organization.id,
          username: value.username.trim(),
          role: value.role,
        };
        if (value.mode === 'existing')
          await request('/api/admin/organizations/members/add', session, common);
        else {
          const profile = {
            ...common,
            display_name: value.display_name.trim(),
            email: value.email.trim(),
          };
          const key = JSON.stringify(profile);
          if (pendingCreate.current?.key !== key)
            pendingCreate.current = { key, id: crypto.randomUUID() };
          await request('/api/admin/organizations/members/create', session, {
            ...profile,
            request_id: pendingCreate.current.id,
            temporary_password: value.temporary_password,
          });
        }
      }
      closeEditor();
      await loadMembers(organization);
      setNotice(member ? 'Изменения сохранены.' : 'Сотрудник добавлен.');
    });
  }
  async function saveRole(value: Pick<OrganizationRole, 'name' | 'description' | 'permissions'>) {
    if (!selected || editor?.kind !== 'role') return;
    const organization = selected;
    const role = editor.role;
    await run(async () => {
      const id = role?.id ?? newRoleId.current;
      await request('/api/admin/organizations/roles/' + (role ? 'update' : 'create'), session, {
        organization_id: organization.id,
        id,
        ...value,
        ...(role ? { expected_revision: role.revision } : {}),
      });
      closeEditor();
      await loadRoles(organization);
      setNotice(role ? 'Роль обновлена.' : 'Роль создана.');
    });
  }
  async function saveOrganization(name: string, enabled: boolean) {
    if (editor?.kind !== 'organization') return;
    const previous = editor.organization;
    await run(async () => {
      const { organization } = await request<{ organization: Organization }>(
        '/api/admin/organizations/' + (previous ? 'update' : 'create'),
        session,
        previous
          ? { id: previous.id, name, enabled, expected_revision: previous.revision }
          : { id: crypto.randomUUID(), name },
      );
      closeEditor();
      await loadOrganizations();
      setSelected(organization);
      setNotice(
        previous
          ? 'Организация обновлена.'
          : 'Организация создана. Добавьте первого администратора.',
      );
    });
  }
  const roleName = (id: string) => roles.find((role) => role.id === id)?.name ?? id;
  return (
    <section className="organizations-workspace">
      <div className="heading">
        <div>
          {selected && access.platform_admin && (
            <button
              className="organization-back"
              onClick={() => {
                openOrganization(undefined);
              }}
            >
              ← Все организации
            </button>
          )}
          <p className="eyebrow">{selected ? 'ОРГАНИЗАЦИЯ' : 'АДМИНИСТРИРОВАНИЕ'}</p>
          <h1>{selected?.name ?? 'Организации'}</h1>
          {selected ? (
            <span className={`organization-status ${selected.enabled ? 'active' : ''}`}>
              {selected.enabled ? 'Активна' : 'Отключена'}
            </span>
          ) : (
            <p>Организации платформы и управление доступом.</p>
          )}
        </div>
        <div className="organization-heading-actions">
          {!selected && access.platform_admin && (
            <button className="primary" onClick={() => openEditor({ kind: 'organization' })}>
              Создать организацию
            </button>
          )}
          {selected && access.platform_admin && (
            <button onClick={() => openEditor({ kind: 'organization', organization: selected })}>
              Изменить организацию
            </button>
          )}
          {!access.platform_admin && ownOrganizations.length > 1 && (
            <label>
              Рабочая организация
              <select
                value={selected?.id ?? ''}
                onChange={(event) =>
                  openOrganization(ownOrganizations.find((item) => item.id === event.target.value))
                }
              >
                {ownOrganizations.map((item) => (
                  <option key={item.id} value={item.id}>
                    {item.name}
                  </option>
                ))}
              </select>
            </label>
          )}
        </div>
      </div>
      {!editor && error && (
        <div role="alert" className="organization-error">
          {error}
        </div>
      )}
      {notice && (
        <div role="status" className="organization-notice">
          {notice}
        </div>
      )}
      {!selected ? (
        access.platform_admin ? (
          <div className="organization-registry">
            <table>
              <thead>
                <tr>
                  <th>Организация</th>
                  <th>Статус</th>
                  <th />
                </tr>
              </thead>
              <tbody>
                {organizations.map((organization) => (
                  <tr
                    key={organization.id}
                    className="organization-row"
                    onClick={() => {
                      openOrganization(organization);
                    }}
                  >
                    <td>
                      <button className="organization-open">{organization.name}</button>
                    </td>
                    <td>
                      <span
                        className={`organization-status ${organization.enabled ? 'active' : ''}`}
                      >
                        {organization.enabled ? 'Активна' : 'Отключена'}
                      </span>
                    </td>
                    <td aria-hidden="true" className="organization-row-arrow">
                      →
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
            {!organizations.length && <p>Организаций пока нет.</p>}
            {hasMore && (
              <button disabled={busy} onClick={() => void run(() => loadOrganizations(true))}>
                Показать ещё
              </button>
            )}
          </div>
        ) : (
          <p>Нет организаций, в которых вам разрешено управлять сотрудниками.</p>
        )
      ) : (
        <>
          <div className="organization-section-tabs" role="tablist" aria-label="Организация">
            <button role="tab" aria-selected={tab === 'members'} onClick={() => setTab('members')}>
              Сотрудники
            </button>
            <button role="tab" aria-selected={tab === 'roles'} onClick={() => setTab('roles')}>
              Роли и доступы
            </button>
          </div>
          <section className="organization-content" role="tabpanel">
            <div className="organization-content-heading">
              <h2>{tab === 'members' ? 'Сотрудники' : 'Роли и доступы'}</h2>
              <div className="actions">
                <button
                  disabled={busy || loading}
                  onClick={() =>
                    void run(() =>
                      tab === 'members' ? loadMembers(selected) : loadRoles(selected),
                    )
                  }
                >
                  Обновить
                </button>
                <button
                  className="primary"
                  disabled={busy || loading}
                  onClick={() => openEditor({ kind: tab === 'members' ? 'member' : 'role' })}
                >
                  {tab === 'members' ? 'Добавить сотрудника' : 'Создать роль'}
                </button>
              </div>
            </div>
            {loading ? (
              <p role="status">Загрузка…</p>
            ) : tab === 'members' ? (
              <>
                <div className="members-table">
                  <table>
                    <thead>
                      <tr>
                        <th>Сотрудник</th>
                        <th>Роль</th>
                        <th>Доступ</th>
                        <th />
                      </tr>
                    </thead>
                    <tbody>
                      {members.map((member) => (
                        <tr
                          key={member.subject}
                          className="organization-row"
                          onClick={() => openEditor({ kind: 'member', member })}
                        >
                          <td>
                            <button className="organization-open">{member.display_name}</button>
                            <small>
                              {member.username} · {member.email}
                            </small>
                          </td>
                          <td>{roleName(member.role)}</td>
                          <td>
                            <span
                              className={`organization-status ${member.enabled ? 'active' : ''}`}
                            >
                              {member.enabled ? 'Разрешён' : 'Отключён'}
                            </span>
                          </td>
                          <td>
                            <button>Изменить</button>
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
                {!members.length && (
                  <div className="organization-empty">
                    <h3>Добавьте первого сотрудника</h3>
                    <p>Начните с администратора, который будет управлять командой.</p>
                  </div>
                )}
                {moreMembers && (
                  <button
                    disabled={busy}
                    onClick={() => void run(() => loadMembers(selected, true))}
                  >
                    Показать ещё
                  </button>
                )}
              </>
            ) : (
              <div className="organization-role-list">
                {roles.map((role) => (
                  <button
                    key={role.id}
                    className="organization-role-card"
                    onClick={() => openEditor({ kind: 'role', role })}
                  >
                    <strong>{role.name}</strong>
                    <span>{role.description || 'Без описания'}</span>
                    <small>
                      Разрешений: {role.permissions.length}
                      <span aria-hidden="true">Изменить →</span>
                    </small>
                  </button>
                ))}
              </div>
            )}
          </section>
        </>
      )}
      {editor?.kind === 'member' && (
        <MemberEditor
          key={editor.member?.subject ?? 'new-member'}
          member={editor.member}
          roles={roles}
          busy={busy}
          error={error}
          save={saveMember}
          close={closeEditor}
        />
      )}
      {editor?.kind === 'role' && (
        <RoleEditor
          key={editor.role?.id ?? 'new-role'}
          role={editor.role}
          permissions={permissions}
          busy={busy}
          error={error}
          save={saveRole}
          close={closeEditor}
        />
      )}
      {editor?.kind === 'organization' && (
        <OrganizationEditor
          organization={editor.organization}
          busy={busy}
          error={error}
          save={saveOrganization}
          close={closeEditor}
        />
      )}
    </section>
  );
}

function OrganizationEditor({
  organization,
  busy,
  error,
  save,
  close,
}: {
  organization?: Organization;
  busy: boolean;
  error: string;
  save: (name: string, enabled: boolean) => Promise<void>;
  close: () => void;
}) {
  const [name, setName] = useState(organization?.name ?? '');
  const [enabled, setEnabled] = useState(organization?.enabled ?? true);
  const dirty = name !== (organization?.name ?? '') || enabled !== (organization?.enabled ?? true);
  return (
    <EditorPanel
      title={organization ? 'Изменить организацию' : 'Новая организация'}
      dirty={dirty}
      busy={busy}
      close={close}
    >
      <form
        className="organization-form"
        onSubmit={(event) => {
          event.preventDefault();
          void save(name.trim(), enabled);
        }}
      >
        {error && (
          <div role="alert" className="organization-error">
            {error}
          </div>
        )}
        <label>
          Название организации
          <input
            value={name}
            required
            maxLength={200}
            disabled={busy}
            onChange={(event) => setName(event.target.value)}
          />
        </label>
        {organization && (
          <label className="member-access">
            <input
              type="checkbox"
              checked={enabled}
              disabled={busy}
              onChange={(event) => setEnabled(event.target.checked)}
            />
            Организация активна
          </label>
        )}
        <button className="primary" disabled={busy || !name.trim() || !dirty}>
          {busy ? 'Сохраняем…' : organization ? 'Сохранить изменения' : 'Создать организацию'}
        </button>
      </form>
    </EditorPanel>
  );
}
