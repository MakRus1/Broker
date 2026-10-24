import { useEffect, useState } from 'react';
import { RouteLocations } from './components/RouteLocations';
import { RailwayStations } from './components/RailwayStations';
import { request } from './api';
import { Organizations, type OrganizationAccess } from './components/Organizations';
import { useWorkspace } from './hooks/useWorkspace';
import { LoginScreen } from './components/LoginScreen';
import { Catalog } from './components/Catalog';
import { ConfigDrawer } from './components/ConfigDrawer';
import { Notifications } from './components/Notifications';

export function App() {
  const workspace = useWorkspace();
  const [section, setSection] = useState<'configs' | 'organizations' | 'stations' | 'locations'>(
    'configs',
  );
  const [access, setAccess] = useState<OrganizationAccess>();
  const [accessError, setAccessError] = useState('');
  useEffect(() => {
    if (!workspace.session) return;
    let active = true;
    request<OrganizationAccess>('/api/organizations/me')
      .then((value) => {
        if (!active) return;
        setAccess(value);
        if (!workspace.session?.permissions.includes('configs.read')) setSection('organizations');
      })
      .catch((reason: unknown) => {
        if (active)
          setAccessError(
            reason instanceof Error ? reason.message : 'Не удалось загрузить права управления',
          );
      });
    return () => {
      active = false;
    };
  }, [workspace.session]);
  const { session, selected, catalog, busy, run, logout } = workspace;
  if (!session) return <LoginScreen workspace={workspace} />;
  return (
    <main className="workspace">
      <header>
        <div className="brand">
          B<span>Broker</span>
        </div>
        <div className="account">
          <span title={session.actor}>{session.display_name ?? session.subject}</span>
          <button disabled={busy} onClick={() => run(logout)}>
            Выйти
          </button>
        </div>
      </header>
      <nav className="admin-navigation" aria-label="Разделы администрирования">
        {session.permissions.includes('configs.read') && (
          <button
            aria-current={section === 'configs' ? 'page' : undefined}
            onClick={() => setSection('configs')}
          >
            Конфиги
          </button>
        )}
        {access && (access.platform_admin || access.managed_organization_ids.length > 0) && (
          <button
            aria-current={section === 'organizations' ? 'page' : undefined}
            onClick={() => setSection('organizations')}
          >
            {access.platform_admin ? 'Организации' : 'Сотрудники и роли'}
          </button>
        )}
        {access?.platform_admin && (
          <button
            aria-current={section === 'stations' ? 'page' : undefined}
            onClick={() => setSection('stations')}
          >
            Станции
          </button>
        )}
        {access?.platform_admin && (
          <button
            aria-current={section === 'locations' ? 'page' : undefined}
            onClick={() => setSection('locations')}
          >
            Порты и аэропорты
          </button>
        )}
      </nav>
      {section === 'locations' && access?.platform_admin ? (
        <RouteLocations session={session} />
      ) : section === 'stations' && access?.platform_admin ? (
        <RailwayStations session={session} />
      ) : section === 'organizations' ? (
        access ? (
          <Organizations session={session} access={access} />
        ) : (
          <p role={accessError ? 'alert' : 'status'}>{accessError || 'Загрузка прав…'}</p>
        )
      ) : (
        <>
          <div className="heading">
            <div>
              <p className="eyebrow">АДМИНИСТРИРОВАНИЕ</p>
              <h1>
                Конфиги <span className="count">{catalog.length}</span>
              </h1>
              <p>Изменения под контролем. Значения, согласование и история — в одном месте.</p>
            </div>
          </div>
          <Catalog workspace={workspace} />
          {selected && <ConfigDrawer key={selected} workspace={workspace} />}
        </>
      )}
      <Notifications workspace={workspace} />
    </main>
  );
}
