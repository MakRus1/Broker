import { useCallback, useEffect, useRef, useState, useSyncExternalStore } from 'react';
import { useLocalization } from '../localization/Localization';
import { Dialog } from '../../shared/ui/Dialog';
import { LookupField } from '../../shared/ui/LookupField';
import { RegistryRoleDefaults } from './RegistryRoleDefaults';
import { restoreColumns, useRegistryPreference } from './registryPreferences';
import type { RegistryColumns } from './RegistryTable';
import { maxSavedViews, restoreSavedViews, sameViewState, snapshotView } from './savedViews';
import { usePreferences } from '../preferences/UserPreferences';
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
  const [action, setAction] = useState<'create' | 'copy' | 'rename'>('create');
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
  const store = usePreferences();
  useSyncExternalStore(store?.subscribe ?? (() => () => {}), store?.snapshot ?? (() => 0), () => 0);
  const saved = selected ?? personal[0];
  const dirty = Boolean(
    binding &&
    (!saved ||
      !sameViewState(
        { query: saved.query, columns: restoreColumns(saved.columns, state.definitions) },
        { query: binding.query, columns: state.columns },
      )),
  );
  const saveState = store?.error
    ? 'views.saveFailed'
    : dirty ||
        store?.hasPending(selected ? (state.viewsKey ?? '') : (state.viewsKey ?? '') + ':personal')
      ? 'views.saving'
      : 'views.saved';
  const previousSaveState = useRef(saveState);
  const [showSaved, setShowSaved] = useState(false);
  useEffect(() => {
    const previous = previousSaveState.current;
    previousSaveState.current = saveState;
    if (saveState !== 'views.saved') {
      setShowSaved(false);
      return;
    }
    if (previous === 'views.saved') return;
    setShowSaved(true);
    const timer = setTimeout(() => setShowSaved(false), 2500);
    return () => clearTimeout(timer);
  }, [saveState]);
  function openAction(next: 'create' | 'copy' | 'rename') {
    setAction(next);
    setName(next === 'rename' ? (selected?.name ?? '') : '');
    setError('');
    setEditing(true);
  }
  function removeSelected() {
    if (!selected) return;
    setRemoved({
      view: snapshotView(selected.id, selected.name, binding?.query, state.columns),
      active: true,
    });
    setViews((old) => old.filter((view) => view.id !== selected.id));
    setPersonal([snapshotView('personal', 'personal', binding?.query, state.columns)]);
    setSelectedId('');
  }

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
  function save() {
    const replace = action === 'rename';
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
    const current = selected
      ? views.map((item) =>
          item.id === selected.id
            ? snapshotView(selected.id, selected.name, binding?.query, state.columns)
            : item,
        )
      : views;
    const next = replace
      ? current.map((item) => (item.id === view.id ? view : item))
      : [...current, view];
    if (new TextEncoder().encode(JSON.stringify(next)).length > 60000) {
      setError(translate('views.tooLarge'));
      return;
    }
    if (!selected)
      setPersonal([snapshotView('personal', 'personal', binding?.query, state.columns)]);
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
            createInControl
            onCreate={() => openAction('create')}
            createLabel={translate('views.create')}
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
            footer={
              <div className="registry-view-menu-actions">
                {selected && (
                  <>
                    <button type="button" onClick={() => openAction('rename')}>
                      {translate('views.rename')}
                    </button>
                    <button type="button" onClick={() => openAction('copy')}>
                      {translate('views.copy')}
                    </button>
                    <button type="button" onClick={removeSelected}>
                      {translate('views.delete')}
                    </button>
                  </>
                )}
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
      <div className="registry-view-save-status" role="status" data-state={saveState}>
        {saveState === 'views.saving' && translate('views.saving')}
        {saveState === 'views.saved' && showSaved && <>✓ {translate('views.saved')}</>}
        {saveState === 'views.saveFailed' && (
          <>
            <span>{translate('views.saveFailed')}</span>
            <span aria-hidden="true"> · </span>
            <button type="button" onClick={() => void store?.retry()}>
              {translate('filter.retry')}
            </button>
          </>
        )}
      </div>
      {error && !editing && (
        <span role="alert" className="error">
          {error}
        </span>
      )}
      {editing && (
        <Dialog
          className="registry-view-dialog"
          title={translate(
            action === 'rename'
              ? 'views.rename'
              : action === 'copy'
                ? 'views.copy'
                : 'views.create',
          )}
          onClose={() => setEditing(false)}
        >
          <form
            className="registry-view-form"
            onSubmit={(event) => {
              event.preventDefault();
              save();
            }}
          >
            {action !== 'rename' && <p className="hint">{translate('views.hint')}</p>}
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
            {error && (
              <p role="alert" className="error">
                {error}
              </p>
            )}
            <div className="registry-view-footer">
              <button type="button" onClick={() => setEditing(false)}>
                {translate('action.cancel')}
              </button>
              <button type="submit" className="primary">
                {translate(action === 'rename' ? 'action.save' : 'views.create')}
              </button>
            </div>
          </form>
        </Dialog>
      )}
    </div>
  );
}
