import { sectionForCard } from '../../app/workspaceSections';
import { restoreRecentCards } from './recentCards';
import { useEffect, useRef, useState } from 'react';
import type { Session } from '../../shared/api/session';
import type { WorkspaceTab } from '../../app/workspace';
import { useLocalization } from '../localization/Localization';
import { Dialog } from '../../shared/ui/Dialog';
import { Select } from '../../shared/ui/Select';
import { usePreferences } from '../preferences/UserPreferences';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import { permittedSources, searchRequest, type SearchHit } from './searchSources';
import './globalSearch.css';

export function GlobalSearch({
  session,
  tabs,
  route,
}: {
  session: Session;
  tabs: WorkspaceTab[];
  route: string;
}) {
  const { translate } = useLocalization();
  const sources = permittedSources(session);
  const preferences = usePreferences();
  const [recent, setRecent] = useRegistryPreference(
    preferenceKey(session.subject, 'search', 'recent'),
    [],
    restoreRecentCards,
  );
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [kind, setKind] = useState('');
  const [groups, setGroups] = useState<Record<string, SearchHit[]>>({});
  const [failed, setFailed] = useState<string[]>([]);
  const [pending, setPending] = useState(false);
  const [active, setActive] = useState(0);
  const results = useRef<HTMLDivElement>(null);
  const current = tabs.find((tab) => tab.route === route);
  const sourceSignature = sources.map((s) => s.id).join('|');
  useEffect(() => {
    if (!current || !sectionForCard(route, session.permissions)) return;
    const hit = {
      id: current.id,
      title: current.title,
      detail: '',
      route: current.route.replace(/\/(edit|route)$/, ''),
    };
    setRecent((old) => [hit, ...old.filter((item) => item.route !== hit.route)].slice(0, 12));
  }, [route, current?.title, session.subject, session.organization_id]);
  function show() {
    setQuery('');
    setKind('');
    setGroups({});
    setFailed([]);
    setActive(0);
    setOpen(true);
  }
  useEffect(() => {
    function shortcut(event: KeyboardEvent) {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k' && !event.altKey) {
        if (document.querySelector('dialog[open]') && !open) return;
        event.preventDefault();
        if (open) setOpen(false);
        else show();
      }
    }
    window.addEventListener('keydown', shortcut);
    return () => window.removeEventListener('keydown', shortcut);
  }, [open]);
  const term = query.trim();
  useEffect(() => {
    if (!open || term.length < 2) {
      setGroups({});
      setFailed([]);
      setPending(false);
      return;
    }
    const controller = new AbortController();
    setGroups({});
    setFailed([]);
    setPending(true);
    setActive(0);
    const timer = setTimeout(() => {
      const selected = sources.filter((s) => !kind || s.id === kind);
      void Promise.allSettled(
        selected.map(async (source) => {
          try {
            const hits = await source.load(session, term, controller.signal);
            if (!controller.signal.aborted) setGroups((old) => ({ ...old, [source.id]: hits }));
          } catch {
            if (!controller.signal.aborted) setFailed((old) => [...old, source.id]);
          }
        }),
      ).then(() => {
        if (!controller.signal.aborted) setPending(false);
      });
    }, 250);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [open, term, kind, session, sourceSignature]);
  const openHits = tabs
    .filter((tab) => sectionForCard(tab.route, session.permissions))
    .map((tab) => ({ id: tab.id, title: tab.title, detail: '', route: tab.route }));
  const emptyHits = [
    ...openHits,
    ...recent.filter(
      (v) =>
        sectionForCard(v.route, session.permissions) &&
        !openHits.some((tab) => tab.route === v.route),
    ),
  ].filter((v) => !kind || v.route.startsWith('/' + kind + '/'));
  const hits = term.length < 2 ? emptyHits : sources.flatMap((source) => groups[source.id] ?? []);
  useEffect(() => {
    results.current
      ?.querySelector<HTMLElement>(`[data-search-index="${active}"]`)
      ?.scrollIntoView({ block: 'nearest' });
  }, [active]);
  function choose(hit: SearchHit) {
    setOpen(false);
    location.hash = hit.route;
  }
  function all(id: string) {
    const source = sources.find((s) => s.id === id);
    if (!source) return;
    const key = preferenceKey(session.subject, id, 'query');
    preferences?.set(preferenceKey(session.subject, id, 'saved-views') + ':active', '');
    preferences?.set(key, searchRequest(source, term));
    setOpen(false);
    location.hash = '/' + id;
  }
  let index = 0;
  function result(hit: SearchHit) {
    const n = index++;
    return (
      <button
        type="button"
        key={hit.route}
        data-search-index={n}
        className={n === active ? 'search-hit active' : 'search-hit'}
        onMouseEnter={() => setActive(n)}
        onClick={() => choose(hit)}
      >
        <strong>{hit.title}</strong>
        <span>
          {hit.detail || translate(sectionForCard(hit.route, session.permissions)!.title)}
        </span>
      </button>
    );
  }
  return (
    <>
      <button
        className="global-search-trigger"
        type="button"
        onClick={show}
        aria-keyshortcuts="Control+k Meta+k"
      >
        {translate('search.title')} <kbd>⌘K / Ctrl K</kbd>
      </button>
      {open && (
        <Dialog
          className="global-search-dialog"
          title={translate('search.title')}
          onClose={() => setOpen(false)}
        >
          <div className="global-search-inputs">
            <label>
              {translate('search.query')}
              <input
                autoFocus
                value={query}
                maxLength={128}
                placeholder={translate('search.placeholder')}
                onChange={(e) => setQuery(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
                    e.preventDefault();
                    setActive((old) =>
                      hits.length
                        ? (old + (e.key === 'ArrowDown' ? 1 : -1) + hits.length) % hits.length
                        : 0,
                    );
                  }
                  if (e.key === 'Enter' && hits[active]) {
                    e.preventDefault();
                    choose(hits[active]);
                  }
                }}
              />
            </label>
            <Select
              label={translate('search.type')}
              value={kind}
              options={[
                { value: '', label: translate('search.all') },
                ...sources.map((s) => ({ value: s.id, label: translate(s.label) })),
              ]}
              onChange={setKind}
            />
          </div>
          <div className="global-search-results" ref={results} aria-busy={pending}>
            {term.length >= 2 && pending && (
              <p role="status" className="hint">
                {translate('common.loading')}
              </p>
            )}
            {term.length >= 2 && !pending && !hits.length && !failed.length && !kind && (
              <p role="status" className="hint">
                {translate('search.empty')}
              </p>
            )}
            {term.length < 2 ? (
              <section>
                <h3>{translate('search.recent')}</h3>
                {emptyHits.map(result)}
                <p className="hint">{translate('search.minimum')}</p>
              </section>
            ) : (
              sources
                .filter(
                  (s) =>
                    (!kind || s.id === kind) &&
                    (groups[s.id]?.length || failed.includes(s.id) || (kind && !pending)),
                )
                .map((source) => (
                  <section key={source.id}>
                    <div className="search-group-heading">
                      <h3>{translate(source.label)}</h3>
                      {!!groups[source.id]?.length && (
                        <button type="button" onClick={() => all(source.id)}>
                          {translate('search.showAll')}
                        </button>
                      )}
                    </div>
                    {(groups[source.id] ?? []).map(result)}
                    {failed.includes(source.id) ? (
                      <p role="status" className="hint">
                        {translate('search.failed')}
                      </p>
                    ) : groups[source.id]?.length === 0 ? (
                      <p className="hint">{translate('search.empty')}</p>
                    ) : (
                      !groups[source.id] && <p className="hint">{translate('common.loading')}</p>
                    )}
                  </section>
                ))
            )}
          </div>
        </Dialog>
      )}
    </>
  );
}
