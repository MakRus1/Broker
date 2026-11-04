import { TranslationEditor } from '../features/localization/TranslationEditor';
import { useEffect, useState } from 'react';
import { RouteLocations } from '../features/directories/RouteLocations';
import { RailwayStations } from '../features/directories/RailwayStations';
import { request } from '../shared/api/api';
import { Organizations, type OrganizationAccess } from '../features/organizations/Organizations';
import { useWorkspace } from './useWorkspace';
import { LoginScreen } from './LoginScreen';
import { Catalog } from '../features/configs/Catalog';
import { ConfigDrawer } from '../features/configs/ConfigDrawer';
import { Notifications } from './Notifications';

import { ThemeControl } from '../features/appearance/ThemeControl';

export function App() {
  const workspace = useWorkspace();
  const [section, setSection] = useState<
    'configs' | 'organizations' | 'stations' | 'locations' | 'translations'
  >('configs');
  const [translationDirty, setTranslationDirty] = useState(false);
  const chooseSection = (next: typeof section) => {
    if (
      next === section ||
      !translationDirty ||
      window.confirm('Перейти без сохранения переводов?')
    )
      setSection(next);
  };
  const [access, setAccess] = useState<OrganizationAccess>();
  const [accessError, setAccessError] = useState('');
  useEffect(() => {
    if (!workspace.session) return;
    let active = true;
    request<OrganizationAccess>('/api/organizations/me')
      .then((value) => {
        if (!active) return;
        setAccess(value);
        if (!workspace.session?.permissions.includes('configs.read'))
          setSection(
            workspace.session?.permissions.includes('localization.read')
              ? 'translations'
              : 'organizations',
          );
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
          <ThemeControl />
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
            onClick={() => chooseSection('configs')}
          >
            Конфиги
          </button>
        )}
        {access && (access.platform_admin || access.managed_organization_ids.length > 0) && (
          <button
            aria-current={section === 'organizations' ? 'page' : undefined}
            onClick={() => chooseSection('organizations')}
          >
            {access.platform_admin ? 'Организации' : 'Сотрудники и роли'}
          </button>
        )}
        {access?.platform_admin && (
          <button
            aria-current={section === 'stations' ? 'page' : undefined}
            onClick={() => chooseSection('stations')}
          >
            Станции
          </button>
        )}
        {access?.platform_admin && (
          <button
            aria-current={section === 'locations' ? 'page' : undefined}
            onClick={() => chooseSection('locations')}
          >
            Порты и аэропорты
          </button>
        )}
        {session.permissions.includes('localization.read') && (
          <button
            aria-current={section === 'translations' ? 'page' : undefined}
            onClick={() => chooseSection('translations')}
          >
            Переводы
          </button>
        )}
      </nav>
      {section === 'translations' && session.permissions.includes('localization.read') ? (
        <TranslationEditor
          session={session}
          organizations={access?.organizations ?? []}
          onDirtyChange={setTranslationDirty}
        />
      ) : section === 'locations' && access?.platform_admin ? (
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
              <p>Значения конфигов, согласование изменений и история.</p>
            </div>
          </div>
          <Catalog workspace={workspace} />
          {(selected || workspace.creating) && (
            <ConfigDrawer key={selected || 'new'} workspace={workspace} />
          )}
        </>
      )}
      <Notifications workspace={workspace} />
    </main>
  );
}
