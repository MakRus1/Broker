import { useCallback, useEffect, useState } from 'react';
import { useLocalization } from '../localization/Localization';
import { Dialog } from '../../shared/ui/Dialog';
import { LookupField } from '../../shared/ui/LookupField';
import { RegistryRoleDefaults } from './RegistryRoleDefaults';
import { restoreColumns, useRegistryPreference } from './registryPreferences';
import type { RegistryColumns } from './RegistryTable';
import { maxSavedViews, restoreSavedViews, sameViewState, snapshotView } from './savedViews';
import './savedViews.css';

export function RegistrySavedViews({ state }: { state: RegistryColumns }) {
  const { translate } = useLocalization();
  const [views, setViews] = useRegistryPreference(state.viewsKey ?? '', [], restoreSavedViews);
  const [selectedId, setSelectedId] = useRegistryPreference(
    state.viewsKey ? state.viewsKey + ':active' : '',
    '',
    (raw) => (typeof raw === 'string' ? raw : ''),
  );
  const [personal, setPersonal] = useRegistryPreference(
    state.viewsKey ? state.viewsKey + ':personal' : '',
    [],
    restoreSavedViews,
  );
  const [editing, setEditing] = useState(false);
  const [name, setName] = useState('');
  const [error, setError] = useState('');
  const [saveCopy, setSaveCopy] = useState(false);
  const [removed, setRemoved] = useState<{ view: (typeof views)[number]; active: boolean }>();
  const sourceLabel = translate(
    state.inheritance?.source() === 'role'
      ? 'views.role'
      : state.inheritance?.source() === 'builtin'
        ? 'views.builtin'
        : 'views.current',
  );
  const choices = [{ id: '__current', name: sourceLabel }, ...views];
  const loadChoices = useCallback(
    async (query: string) => ({
      items: choices.filter((item) =>
        item.name.toLocaleLowerCase().includes(query.toLocaleLowerCase()),
      ),
    }),
    [JSON.stringify(choices)],
  );
  const selected = views.find((view) => view.id === selectedId);
  const binding = state.view;
  const currentSignature = JSON.stringify({ query: binding?.query, columns: state.columns });
  useEffect(() => {
    if (!binding || binding.active === false || !state.viewsKey) return;
    const saved = selected ?? personal[0];
    if (
      saved &&
      sameViewState(
        { query: saved.query, columns: restoreColumns(saved.columns, state.definitions) },
        { query: binding.query, columns: state.columns },
      )
    )
      return;
    const timer = setTimeout(() => {
      const view = snapshotView(
        selected?.id ?? 'personal',
        selected?.name ?? 'personal',
        binding?.query,
        state.columns,
      );
      const next = selected ? views.map((item) => (item.id === selected.id ? view : item)) : [view];
      if (
        JSON.stringify(view).length > 24000 ||
        new TextEncoder().encode(JSON.stringify(next)).length > 60000
      ) {
        setError(translate('views.tooLarge'));
        return;
      }
      if (selected) setViews(next);
      else setPersonal(next);
    }, 400);
    return () => clearTimeout(timer);
  }, [
    state.viewsKey,
    binding?.active,
    selectedId,
    currentSignature,
    JSON.stringify(views),
    JSON.stringify(personal),
  ]);
  if (!binding || !state.viewsKey) return null;
  const validName = name.trim();
  function save(replace: boolean) {
    const duplicate = views.some(
      (view) =>
        (!replace || view.id !== selected?.id) &&
        view.name.toLocaleLowerCase() === validName.toLocaleLowerCase(),
    );
    const problem = !validName
      ? 'views.nameRequired'
      : duplicate
        ? 'views.duplicate'
        : !replace && views.length >= maxSavedViews
          ? 'views.limit'
          : undefined;
    if (problem) {
      setError(translate(problem));
      return;
    }
    const view = snapshotView(
      replace && selected ? selected.id : crypto.randomUUID(),
      validName,
      binding?.query,
      state.columns,
    );
    if (JSON.stringify(view).length > 24000) {
      setError(translate('views.tooLarge'));
      return;
    }
    const next = replace
      ? views.map((item) => (item.id === view.id ? view : item))
      : [...views, view];
    if (new TextEncoder().encode(JSON.stringify(next)).length > 60000) {
      setError(translate('views.tooLarge'));
      return;
    }
    setViews(next);
    setSelectedId(view.id);
    setEditing(false);
  }
  return (
    <div className="registry-saved-views">
      <RegistryRoleDefaults
        state={state}
        trigger={(openRole) => (
          <LookupField
            label={translate('views.title')}
            value={selected?.id ?? '__current'}
            selectedLabel={selected?.name ?? sourceLabel}
            placeholder={selected?.name ?? sourceLabel}
            popupClassName="registry-view-popup"
            load={loadChoices}
            itemLabel={(item) => item.name}
            clearable={false}
            onChange={(item) => {
              if (!item) return;
              const id = item.id === '__current' ? '' : item.id;
              const previous = snapshotView(
                selected?.id ?? 'personal',
                selected?.name ?? 'personal',
                binding.query,
                state.columns,
              );
              if (selected)
                setViews((old) =>
                  old.map((entry) => (entry.id === selected.id ? previous : entry)),
                );
              else setPersonal([previous]);
              const view =
                views.find((entry) => entry.id === id) ?? (!id ? personal[0] : undefined);
              setSelectedId(id);
              if (!view) return;
              binding.apply(view.query);
              state.setColumns(restoreColumns(view.columns, state.definitions));
            }}
            itemActions={(item) =>
              item.id !== '__current' && (
                <button
                  type="button"
                  className="registry-view-remove"
                  aria-label={translate('views.deleteNamed', { name: item.name })}
                  title={translate('views.deleteNamed', { name: item.name })}
                  onMouseDown={(event) => event.preventDefault()}
                  onClick={() => {
                    const view = views.find((entry) => entry.id === item.id);
                    if (!view) return;
                    setRemoved({ view, active: selectedId === view.id });
                    setViews((old) => old.filter((entry) => entry.id !== view.id));
                    if (selectedId === view.id) setSelectedId('');
                  }}
                >
                  <svg
                    width="16"
                    height="16"
                    viewBox="0 0 24 24"
                    fill="none"
                    stroke="currentColor"
                    strokeWidth="1.8"
                    aria-hidden="true"
                  >
                    <path d="M3 6h18M9 6V3h6v3M5 6l1 15h12l1-15M10 10v7m4-7v7" />
                  </svg>
                </button>
              )
            }
            footer={
              <div className="registry-view-menu-actions">
                <button
                  type="button"
                  onClick={() => {
                    setName(selected?.name ?? '');
                    setSaveCopy(true);
                    setError('');
                    setEditing(true);
                  }}
                >
                  {translate('views.save')}
                </button>
                {state.inheritance && (selected || state.inheritance.source() === 'personal') && (
                  <button
                    type="button"
                    onClick={() => {
                      setSelectedId('');
                      setPersonal([]);
                      state.inheritance!.reset();
                    }}
                  >
                    {translate(
                      state.inheritance.inheritedSource() === 'role'
                        ? 'views.inheritRole'
                        : 'views.inheritBuiltin',
                    )}
                  </button>
                )}
                {openRole && (
                  <button type="button" onClick={openRole}>
                    {translate('roleView.action')}
                  </button>
                )}
                {removed && (
                  <button
                    type="button"
                    onClick={() => {
                      setViews((old) => [...old, removed.view]);
                      if (removed.active) setSelectedId(removed.view.id);
                      setRemoved(undefined);
                    }}
                  >
                    {translate('views.undoDelete', { name: removed.view.name })}
                  </button>
                )}
              </div>
            }
          />
        )}
      />
      {error && !editing && (
        <span role="alert" className="error">
          {error}
        </span>
      )}
      {editing && (
        <Dialog
          className="registry-view-dialog"
          title={translate('views.manage')}
          onClose={() => setEditing(false)}
        >
          <form
            className="registry-view-form"
            onSubmit={(event) => {
              event.preventDefault();
              save(!!selected && !saveCopy);
            }}
          >
            <p className="hint">{translate('views.hint')}</p>
            <label>
              {translate('views.name')}
              <input
                autoFocus
                maxLength={64}
                value={name}
                onChange={(event) => {
                  setName(event.target.value);
                  setError('');
                }}
              />
            </label>
            {selected && (
              <label className="registry-view-copy">
                <input
                  type="checkbox"
                  checked={saveCopy}
                  onChange={(event) => setSaveCopy(event.target.checked)}
                />
                {translate('views.saveCopy')}
              </label>
            )}
            {error && (
              <p role="alert" className="error">
                {error}
              </p>
            )}
            <div className="registry-view-footer">
              {selected && (
                <button
                  type="button"
                  className="registry-view-delete"
                  onClick={() => {
                    setViews((old) => old.filter((view) => view.id !== selected.id));
                    setSelectedId('');
                    setEditing(false);
                  }}
                >
                  {translate('views.delete')}
                </button>
              )}
              <button type="button" onClick={() => setEditing(false)}>
                {translate('action.cancel')}
              </button>
              <button type="submit" className="primary">
                {translate('action.save')}
              </button>
            </div>
          </form>
        </Dialog>
      )}
    </div>
  );
}
