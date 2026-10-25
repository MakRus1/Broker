import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { Catalog } from './components/Catalog';
import { CurrentValue } from './components/CurrentValue';
import type { Workspace } from './hooks/useWorkspace';

const workspace = (values: Partial<Workspace>) => values as Workspace;

describe('создание и удаление конфигов', () => {
  it('предлагает создание по праву записи, независимо от должности', () => {
    const render = (write: boolean) =>
      renderToStaticMarkup(<Catalog workspace={workspace({ catalog: [], write })} />);
    expect(render(true)).toContain('Создать конфиг');
    expect(render(false)).not.toContain('Создать конфиг');
  });

  it('не предлагает редактировать отсутствующее опубликованное значение', () => {
    const html = renderToStaticMarkup(
      <CurrentValue
        workspace={workspace({
          current: { config_name: 'NEW_CONFIG', version: 0, value: null },
          write: true,
          editor: false,
        })}
      />,
    );
    expect(html).toContain('после подтверждения');
    expect(html).not.toContain('Редактировать');
    expect(html).not.toContain('Удалить');
  });

  it('показывает восстановление удалённого конфига через историю', () => {
    const html = renderToStaticMarkup(
      <CurrentValue
        workspace={workspace({
          current: { config_name: 'OLD_CONFIG', version: 3, value: null },
          write: true,
          editor: false,
        })}
      />,
    );
    expect(html).toContain('восстановить из истории');
    expect(html).not.toContain('Редактировать');
  });
});
