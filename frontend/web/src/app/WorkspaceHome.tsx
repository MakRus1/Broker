import { useState, useSyncExternalStore } from 'react';
import { useLocalization } from '../features/localization/Localization';
import type { Session } from '../shared/api/session';
import { usePreferences } from '../features/preferences/UserPreferences';
import { preferenceKey, useRegistryPreference } from '../features/registry/registryPreferences';
import { restoreSavedViews, type SavedView } from '../features/registry/savedViews';
import { restoreRecentCards } from '../features/search/recentCards';
import { workspaceGroups, sectionRegistry, sectionForCard } from './workspaceSections';
import {
  favoriteSection,
  favoriteView,
  restoreFavorites,
  visibleSections,
} from './homePreferences';
import './workspaceHome.css';

const noopSubscribe = () => () => {};
export function WorkspaceHome({ session }: { session: Session }) {
  const { translate } = useLocalization();
  const preferences = usePreferences();
  useSyncExternalStore(
    preferences?.subscribe ?? noopSubscribe,
    preferences?.snapshot ?? (() => 0),
    () => 0,
  );
  const [favorites, setFavorites] = useRegistryPreference(
    preferenceKey(session.subject, 'home', 'favorites'),
    [],
    restoreFavorites,
  );
  const [recent] = useRegistryPreference(
    preferenceKey(session.subject, 'search', 'recent'),
    [],
    restoreRecentCards,
  );
  const [catalogOpen, setCatalogOpen] = useState(false);
  const sections = visibleSections(session.permissions);
  const views = sections.flatMap((section) => {
    const registry = sectionRegistry(section.route);
    return restoreSavedViews(
      preferences?.read(preferenceKey(session.subject, registry, 'saved-views')),
    ).map((view) => ({ section, registry, view, key: favoriteView(registry, view.id) }));
  });
  const pinned = favorites.flatMap((key) => {
    const section = sections.find((s) => favoriteSection(s.route) === key);
    if (section)
      return [
        {
          key,
          title: translate(section.title),
          kind: translate('home.sectionKind'),
          detail: translate(section.description),
          open: () => {
            location.hash = section.route;
          },
        },
      ];
    const saved = views.find((v) => v.key === key);
    return saved
      ? [
          {
            key,
            title: saved.view.name,
            kind: translate('home.viewKind'),
            detail: translate(saved.section.title),
            open: () => openView(saved.registry, saved.section.route, saved.view),
          },
        ]
      : [];
  });
  function toggle(key: string) {
    setFavorites((old) =>
      old.includes(key) ? old.filter((v) => v !== key) : [...old, key].slice(0, 60),
    );
  }
  function pin(key: string) {
    const selected = favorites.includes(key);
    return (
      <button
        type="button"
        className="home-pin"
        aria-pressed={selected}
        aria-label={translate(selected ? 'home.unpin' : 'home.pin')}
        title={translate(selected ? 'home.unpin' : 'home.pin')}
        onClick={() => toggle(key)}
      >
        <svg viewBox="0 0 24 24" aria-hidden="true" fill={selected ? 'currentColor' : 'none'}>
          <path d="m12 3 2.8 5.7 6.2.9-4.5 4.4 1.1 6.2-5.6-3-5.6 3 1.1-6.2L3 9.6l6.2-.9Z" />
        </svg>
      </button>
    );
  }
  function openView(registry: string, route: string, view: SavedView) {
    preferences?.set(preferenceKey(session.subject, registry, 'query'), view.query);
    preferences?.set(preferenceKey(session.subject, registry, 'columns'), view.columns);
    preferences?.set(preferenceKey(session.subject, registry, 'saved-views') + ':active', view.id);
    location.hash = route;
  }
  return (
    <section className="workspace-home home-dashboard" aria-labelledby="workspace-home-title">
      <h1 id="workspace-home-title">{translate('home.title')}</h1>
      <div className="home-top-grid">
        <section className="home-block" aria-labelledby="home-favorites">
          <h2 id="home-favorites">{translate('home.favorites')}</h2>
          {pinned.length ? (
            <div className="home-list">
              {pinned.map((item) => (
                <div className="home-row" key={item.key}>
                  <button type="button" className="home-record" onClick={item.open}>
                    <span className="home-record-title">
                      <strong>{item.title}</strong>
                      <span className="home-record-kind">{item.kind}</span>
                    </span>
                    <small>{item.detail}</small>
                  </button>
                  {pin(item.key)}
                </div>
              ))}
            </div>
          ) : (
            <div className="home-empty">
              <p>{translate('home.favoritesHint')}</p>
              <button type="button" onClick={() => setCatalogOpen(true)}>
                {translate('home.chooseSections')}
              </button>
            </div>
          )}
        </section>
        <section className="home-block" aria-labelledby="home-recent">
          <h2 id="home-recent">{translate('home.recent')}</h2>
          {recent.some((hit) => sectionForCard(hit.route, session.permissions)) ? (
            <div className="home-list">
              {recent
                .filter((hit) => sectionForCard(hit.route, session.permissions))
                .slice(0, 6)
                .map((hit) => (
                  <a className="home-record" key={hit.route} href={'#' + hit.route}>
                    <strong>{hit.title}</strong>
                    <small>
                      {translate(sectionForCard(hit.route, session.permissions)!.title)}
                    </small>
                  </a>
                ))}
            </div>
          ) : (
            <p className="home-empty">{translate('home.recentHint')}</p>
          )}
        </section>
      </div>
      <section className="home-block home-views" aria-labelledby="home-views">
        <h2 id="home-views">{translate('home.savedViews')}</h2>
        {views.length ? (
          <div className="home-view-list">
            {views.map(({ view, section, registry, key }) => (
              <div className="home-row" key={key}>
                <button
                  type="button"
                  className="home-record"
                  onClick={() => openView(registry, section.route, view)}
                >
                  <strong>{view.name}</strong>
                  <small>{translate(section.title)}</small>
                </button>
                {pin(key)}
              </div>
            ))}
          </div>
        ) : (
          <p className="home-empty">{translate('home.viewsHint')}</p>
        )}
      </section>
      <details
        className="home-catalog"
        open={catalogOpen}
        onToggle={(event) => setCatalogOpen(event.currentTarget.open)}
      >
        <summary>{translate('home.allSections')}</summary>
        <div className="workspace-shortcuts">
          {workspaceGroups.map((group) => {
            const items = group.items.filter((item) =>
              session.permissions.includes(item.permission),
            );
            return items.length ? (
              <section key={group.title}>
                <h2>{translate(group.title)}</h2>
                {items.map((item) => (
                  <div className="home-row" key={item.route}>
                    <a className="home-record" href={'#' + item.route}>
                      <span>
                        <strong>{translate(item.title)}</strong>
                        <small>{translate(item.description)}</small>
                      </span>
                    </a>
                    {pin(favoriteSection(item.route))}
                  </div>
                ))}
              </section>
            ) : null;
          })}
        </div>
      </details>
      {!sections.length && <p>{translate('home.noAccess')}</p>}
    </section>
  );
}
