import type { Permission } from './types';

export function PermissionGroups({
  permissions,
  selected,
  busy,
  onChange,
}: {
  permissions: Permission[];
  selected: string[];
  busy: boolean;
  onChange: (value: string[]) => void;
}) {
  function toggle(id: string, enabled: boolean) {
    const next = new Set(selected);
    if (enabled) {
      function include(code: string) {
        if (next.has(code)) return;
        next.add(code);
        permissions.find((permission) => permission.id === code)?.dependencies.forEach(include);
      }
      include(id);
    } else {
      next.delete(id);
      let changed = true;
      while (changed) {
        changed = false;
        for (const permission of permissions) {
          if (next.has(permission.id) && permission.dependencies.some((code) => !next.has(code))) {
            next.delete(permission.id);
            changed = true;
          }
        }
      }
    }
    onChange([...next]);
  }
  const groups = [...new Set(permissions.map((permission) => permission.group))];
  return (
    <div className="role-permissions">
      <h3>Права и доступы</h3>
      {selected.some((code) => !permissions.some((permission) => permission.id === code)) && (
        <details className="permission-group">
          <summary>Неактивные разрешения</summary>
          <ul>
            {selected
              .filter((code) => !permissions.some((permission) => permission.id === code))
              .map((code) => (
                <li key={code}>{code}</li>
              ))}
          </ul>
        </details>
      )}
      {groups.map((group) => (
        <details key={group} className="permission-group">
          <summary>
            <span>{group}</span>
            <small>
              {
                permissions.filter(
                  (permission) => permission.group === group && selected.includes(permission.id),
                ).length
              }
              {' из '}
              {permissions.filter((permission) => permission.group === group).length}
            </small>
          </summary>
          <fieldset disabled={busy}>
            <legend className="permission-group-legend">{group}</legend>
            {permissions
              .filter((permission) => permission.group === group)
              .map((permission) => (
                <label key={permission.id}>
                  <input
                    type="checkbox"
                    checked={selected.includes(permission.id)}
                    onChange={(event) => toggle(permission.id, event.target.checked)}
                  />
                  {permission.name}
                </label>
              ))}
          </fieldset>
        </details>
      ))}
    </div>
  );
}
