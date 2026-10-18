import { useState } from 'react';
import type { Workspace } from '../hooks/useWorkspace';

export function Catalog({ workspace }: { workspace: Workspace }) {
  const { catalog, setSelected } = workspace;
  const [search, setSearch] = useState('');
  const items = catalog.filter((item) =>
    item.config_name.toLowerCase().includes(search.toLowerCase()),
  );
  return (
    <section className="panel catalog" aria-label="Каталог конфигов">
      <label htmlFor="search">Поиск по названию</label>
      <input
        id="search"
        type="search"
        value={search}
        onChange={(event) => setSearch(event.target.value)}
        placeholder="Найти конфиг…"
      />
      <table>
        <thead>
          <tr>
            <th>Название</th>
            <th>Версия</th>
            <th>Состояние</th>
          </tr>
        </thead>
        <tbody>
          {items.map((item) => (
            <tr key={item.config_name}>
              <td>
                <button className="config-link" onClick={() => setSelected(item.config_name)}>
                  {item.config_name}
                </button>
              </td>
              <td>{item.version}</td>
              <td>
                <span className={'badge ' + (item.active_draft_id ? 'amber' : '')}>
                  {item.active_draft_id ? `Драфт #${item.active_draft_id}` : 'Опубликован'}
                </span>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
      {!items.length && <p className="empty">Конфиги не найдены.</p>}
    </section>
  );
}
