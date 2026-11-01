import { expect, it } from 'vitest';
import { renderToStaticMarkup } from 'react-dom/server';
import { RegistrySelectionActions } from './RegistrySelectionActions';
it('несовместимость оплаты не блокирует независимое скачивание PDF', () => {
  const html = renderToStaticMarkup(
    <RegistrySelectionActions
      count={2}
      actions={[
        { label: 'Распределить поступление', problem: 'Разные контрагенты', onClick: () => {} },
        { label: 'Скачать PDF в ZIP', onClick: () => {} },
      ]}
      onClear={() => {}}
      onCloseMenu={() => {}}
    />,
  );
  expect(html).toMatch(/<button[^>]*disabled[^>]*>Распределить поступление<\/button>/);
  expect(html).toContain('<button>Скачать PDF в ZIP</button>');
});
