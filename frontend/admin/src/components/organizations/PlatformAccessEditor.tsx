import { useEffect, useState } from 'react';
import { request, type Session } from '../../api';
import { EditorPanel } from './EditorPanel';
import { PermissionGroups } from './PermissionGroups';
import type { Member, Permission } from './types';

type Access = { permissions: string[]; revision: number };
type Result = { access: Access; catalog: Permission[] };

export function PlatformAccessEditor({
  member,
  session,
  close,
}: {
  member: Member;
  session: Session;
  close: () => void;
}) {
  const [initial, setInitial] = useState<Access>();
  const [selected, setSelected] = useState<string[]>([]);
  const [catalog, setCatalog] = useState<Permission[]>([]);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const dirty = !!initial && [...initial.permissions].sort().join() !== [...selected].sort().join();
  useEffect(() => {
    let active = true;
    request<Result>('/api/admin/platform-access/get', session, { subject: member.subject })
      .then((result) => {
        if (!active) return;
        setInitial(result.access);
        setSelected(result.access.permissions);
        setCatalog(result.catalog);
      })
      .catch((reason: unknown) => {
        if (active)
          setError(reason instanceof Error ? reason.message : 'Не удалось загрузить права');
      });
    return () => {
      active = false;
    };
  }, [member.subject, session]);
  async function save() {
    if (!initial || busy) return;
    setBusy(true);
    setError('');
    try {
      await request<Result>('/api/admin/platform-access/update', session, {
        subject: member.subject,
        permissions: selected,
        expected_revision: initial.revision,
      });
      if (member.subject === session.subject)
        window.dispatchEvent(new Event('broker-permissions-changed'));
      close();
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Не удалось сохранить права');
    } finally {
      setBusy(false);
    }
  }
  return (
    <EditorPanel
      title={`Права платформы · ${member.display_name}`}
      dirty={dirty}
      busy={busy}
      close={close}
    >
      <form
        className="organization-form"
        onSubmit={(event) => {
          event.preventDefault();
          void save();
        }}
      >
        {error && (
          <div className="organization-error" role="alert">
            {error}
          </div>
        )}
        {!initial ? (
          <p role="status">Загрузка прав…</p>
        ) : (
          <PermissionGroups
            permissions={catalog}
            selected={selected}
            busy={busy}
            onChange={setSelected}
          />
        )}
        <div className="organization-editor-actions">
          <button className="primary" disabled={!dirty || busy}>
            {busy ? 'Сохраняем…' : 'Сохранить права'}
          </button>
        </div>
      </form>
    </EditorPanel>
  );
}
