import { ContractNumberingSettings } from './organizations/ContractNumberingSettings';
import { useEffect, useMemo, useRef, useState } from 'react';
import { request, type Session } from '../api';
import { useRegistry, type RegistryQuery } from '../hooks/useRegistry';
import { RegistryFilters, RegistryMore, SortHeading, EnabledFilter } from './Registry';
import { Select } from './Select';
import { FinanceSettings } from './organizations/FinanceSettings';
import { ShipmentSettings } from './organizations/ShipmentSettings';
import { EditorPanel } from './organizations/EditorPanel';
import { MemberEditor } from './organizations/MemberEditor';
import { PlatformAccessEditor } from './organizations/PlatformAccessEditor';
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
  | { kind: 'platform'; member: Member }
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
  const ownOrganizations = useMemo(
    () => access.organizations.filter((item) => access.managed_organization_ids.includes(item.id)),
    [access],
  );
  const [organizationFilters, setOrganizationFilters] = useState<RegistryQuery>({
    sort: 'name',
    descending: false,
  });
  const [memberFilters, setMemberFilters] = useState<RegistryQuery>({
    sort: 'display_name',
    descending: false,
  });
  const organizationList = useRegistry<Organization>(
    '/api/admin/organizations/list',
    session,
    organizationFilters,
    access.platform_admin,
  );
  const organizations = access.platform_admin ? organizationList.items : ownOrganizations;
  const [selected, setSelected] = useState<Organization | undefined>(
    access.platform_admin ? undefined : ownOrganizations[0],
  );
  const [tab, setTab] = useState<'members' | 'roles' | 'shipments' | 'finance'>('members');
  const memberList = useRegistry<Member>(
    '/api/admin/organizations/members/list',
    session,
    { ...memberFilters, organization_id: selected?.id },
    Boolean(selected),
  );
  const members = memberList.items;
  const [roles, setRoles] = useState<OrganizationRole[]>([]);
  const [permissions, setPermissions] = useState<Permission[]>([]);
  const [editor, setEditor] = useState<Editor>();
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
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
      setSelected((previous) =>
        id
          ? previous?.id === id
            ? previous
            : available.find((item) => item.id === id)
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
  async function loadOrganizations() {
    organizationList.refresh();
  }
  async function loadMembers() {
    memberList.refresh();
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
    if (!selected) return;
    let active = true;
    setLoading(true);
    setRoles([]);
    setError('');
    request<{ items: OrganizationRole[]; permissions: Permission[] }>(
      '/api/admin/organizations/roles/list',
      session,
      { organization_id: selected.id },
    )
      .then((roleData) => {
        if (!active) return;
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
        let revision = member.revision;
        const namesChanged =
          value.last_name.trim() !== (member.last_name ?? '') ||
          value.first_name.trim() !== (member.first_name ?? '') ||
          value.middle_name.trim() !== (member.middle_name ?? '');
        if (namesChanged) {
          const updated = await request<{ member: Member }>(
            '/api/admin/organizations/members/update-profile',
            session,
            {
              organization_id: organization.id,
              subject: member.subject,
              last_name: value.last_name.trim(),
              first_name: value.first_name.trim(),
              middle_name: value.middle_name.trim() || null,
            },
          );
          revision = updated.member.revision;
        }
        if (value.role !== member.role || value.enabled !== member.enabled)
          await request('/api/admin/organizations/members/update', session, {
            organization_id: organization.id,
            subject: member.subject,
            expected_revision: revision,
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
            last_name: value.last_name.trim(),
            first_name: value.first_name.trim(),
            middle_name: value.middle_name.trim() || null,
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
      await loadMembers();
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
      openOrganization(organization);
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
            <RegistryFilters
              value={organizationFilters}
              onApply={setOrganizationFilters}
              loading={organizationList.loading}
              placeholder="Название организации…"
            >
              {(draft, change) => (
                <EnabledFilter
                  value={draft.enabled}
                  onChange={(enabled) => change({ ...draft, enabled })}
                />
              )}
            </RegistryFilters>
            {organizationList.loading && <p role="status">Загрузка…</p>}
            <table>
              <thead>
                <tr>
                  <SortHeading
                    label="Организация"
                    field="name"
                    value={organizationFilters}
                    onChange={setOrganizationFilters}
                  />
                  <SortHeading
                    label="Статус"
                    field="enabled"
                    value={organizationFilters}
                    onChange={setOrganizationFilters}
                  />
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
            {!organizationList.loading && !organizationList.error && !organizations.length && (
              <p>Организации не найдены.</p>
            )}
            <RegistryMore
              hasMore={organizationList.hasMore}
              loading={organizationList.loadingMore}
              error={organizationList.error}
              loadMore={organizationList.loadMore}
            />
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
            <button
              role="tab"
              aria-selected={tab === 'shipments'}
              onClick={() => setTab('shipments')}
            >
              Правила поставок
            </button>
            <button role="tab" aria-selected={tab === 'finance'} onClick={() => setTab('finance')}>
              Финансы
            </button>
          </div>
          {tab === 'finance' ? (
            <>
              <FinanceSettings key={selected.id} session={session} organization={selected.id} />
              <ContractNumberingSettings
                key={'contracts-' + selected.id}
                session={session}
                organization={selected.id}
              />
            </>
          ) : tab === 'shipments' ? (
            <ShipmentSettings key={selected.id} session={session} organization={selected.id} />
          ) : (
            <section className="organization-content" role="tabpanel">
              <div className="organization-content-heading">
                <h2>{tab === 'members' ? 'Сотрудники' : 'Роли и доступы'}</h2>
                <div className="actions">
                  <button
                    disabled={busy || loading}
                    onClick={() =>
                      void run(() => (tab === 'members' ? loadMembers() : loadRoles(selected)))
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
                  <RegistryFilters
                    value={memberFilters}
                    onApply={setMemberFilters}
                    loading={memberList.loading}
                    placeholder="ФИО, логин или почта…"
                  >
                    {(draft, change) => (
                      <>
                        <EnabledFilter
                          value={draft.enabled}
                          onChange={(enabled) => change({ ...draft, enabled })}
                        />
                        <Select
                          label="Роль"
                          value={draft.role ?? ''}
                          options={[
                            { value: '', label: 'Все роли' },
                            ...roles.map((role) => ({ value: role.id, label: role.name })),
                          ]}
                          onChange={(role) => change({ ...draft, role: role || undefined })}
                        />
                      </>
                    )}
                  </RegistryFilters>
                  {memberList.loading && <p role="status">Загрузка…</p>}
                  <div className="members-table">
                    <table>
                      <thead>
                        <tr>
                          <SortHeading
                            label="Сотрудник"
                            field="display_name"
                            value={memberFilters}
                            onChange={setMemberFilters}
                          />
                          <SortHeading
                            label="Роль"
                            field="role"
                            value={memberFilters}
                            onChange={setMemberFilters}
                          />
                          <SortHeading
                            label="Доступ"
                            field="enabled"
                            value={memberFilters}
                            onChange={setMemberFilters}
                          />
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
                  {!memberList.loading && !memberList.error && !members.length && (
                    <div className="organization-empty">
                      <h3>Сотрудники не найдены</h3>
                      <p>Измените фильтры или добавьте сотрудника.</p>
                    </div>
                  )}
                  <RegistryMore
                    hasMore={memberList.hasMore}
                    loading={memberList.loadingMore}
                    error={memberList.error}
                    loadMore={memberList.loadMore}
                  />
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
          )}
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
          platformAccess={
            access.platform_admin && editor.member
              ? () => {
                  if (editor.member) setEditor({ kind: 'platform', member: editor.member });
                }
              : undefined
          }
          close={closeEditor}
        />
      )}
      {editor?.kind === 'platform' && (
        <PlatformAccessEditor member={editor.member} session={session} close={closeEditor} />
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
