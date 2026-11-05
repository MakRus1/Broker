import { useState, type ReactNode } from 'react';
import { useLocalization } from '../localization/Localization';
import { usePreferencesSession, type RoleRegistryDefault } from '../preferences/UserPreferences';
import { request } from '../../shared/api/http';
import { Dialog, DialogActions } from '../../shared/ui/Dialog';
import { Select } from '../../shared/ui/Select';
import type { RegistryColumns } from './RegistryTable';
export function RegistryRoleDefaults({
  state,
  trigger,
}: {
  state: RegistryColumns;
  trigger?: (open?: () => void) => ReactNode;
}) {
  const { translate } = useLocalization();
  const [session] = usePreferencesSession();
  const [open, setOpen] = useState(false);
  const [roles, setRoles] = useState<{ id: string; name: string }[]>([]);
  const [items, setItems] = useState<RoleRegistryDefault[]>([]);
  const [role, setRole] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  if (!session?.permissions.includes('organizations.manage') || !state.view || !state.registry)
    return trigger?.() ?? null;
  async function start() {
    setOpen(true);
    setBusy(true);
    setError('');
    setNotice('');
    try {
      const data = await request<{ roles: typeof roles; items: RoleRegistryDefault[] }>(
        '/api/preferences/roles/list',
        session!,
        { organization_id: session!.organization_id },
      );
      setRoles(data.roles);
      setItems(data.items);
      setRole(data.roles.some((item) => item.id === role) ? role : (data.roles[0]?.id ?? ''));
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  async function save(reset = false) {
    setBusy(true);
    setError('');
    try {
      await request('/api/preferences/roles/save', session!, {
        organization_id: session!.organization_id,
        role,
        registry: state.registry,
        expected_revision:
          items.find((item) => item.role === role && item.registry === state.registry)?.revision ??
          0,
        value: reset ? {} : { columns: state.columns, query: state.view!.query },
      });
      setOpen(false);
      setNotice(translate('roleView.saved'));
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  return (
    <>
      {trigger ? (
        trigger(() => void start())
      ) : (
        <button type="button" onClick={() => void start()}>
          {translate('roleView.action')}
        </button>
      )}
      {notice && !trigger && (
        <span role="status" className="hint">
          {notice}
        </span>
      )}
      {open && (
        <Dialog
          title={translate('roleView.title')}
          onClose={() => {
            if (!busy) setOpen(false);
          }}
        >
          <p className="hint">{translate('roleView.description')}</p>
          <Select
            label={translate('roleView.role')}
            value={role}
            options={roles.map((item) => ({ value: item.id, label: item.name }))}
            disabled={busy}
            onChange={setRole}
          />
          {error && (
            <div role="alert" className="error">
              <p>{error}</p>
              <button type="button" disabled={busy} onClick={() => void start()}>
                {translate('ui.retryLoad')}
              </button>
            </div>
          )}
          <DialogActions>
            <button
              type="button"
              disabled={
                busy ||
                !!error ||
                !items.some(
                  (item) =>
                    item.role === role &&
                    item.registry === state.registry &&
                    item.value.columns !== undefined,
                )
              }
              onClick={() => void save(true)}
            >
              {translate('roleView.reset')}
            </button>
            <button type="button" disabled={busy} onClick={() => setOpen(false)}>
              {translate('action.cancel')}
            </button>
            <button
              type="button"
              className="primary"
              disabled={busy || !role || !!error}
              onClick={() => void save()}
            >
              {translate('action.save')}
            </button>
          </DialogActions>
        </Dialog>
      )}
    </>
  );
}
