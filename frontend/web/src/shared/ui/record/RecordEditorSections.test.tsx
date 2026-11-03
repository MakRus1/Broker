import { expect, it } from 'vitest';
import { renderToStaticMarkup } from 'react-dom/server';
import { activeEditorSection, RecordEditorSections } from './RecordEditorSections';
import { RecordSection } from './RecordLayout';

it('tracks the last section above the navigation without skipping the first', () => {
  const sections = [
    { id: 'parties', top: -200 },
    { id: 'amount', top: 120 },
    { id: 'note', top: 700 },
  ];
  expect(activeEditorSection(sections, 80)).toBe('parties');
  expect(activeEditorSection(sections, 140)).toBe('amount');
  expect(activeEditorSection(sections, 800)).toBe('note');
  expect(activeEditorSection([], 80)).toBeUndefined();
});
it('retains semantic section headings and keyboard focus targets', () => {
  const html = renderToStaticMarkup(
    <RecordEditorSections>
      <RecordSection title="Стороны">
        <input aria-label="Клиент" />
      </RecordSection>
    </RecordEditorSections>,
  );
  expect(html).toContain('aria-labelledby=');
  expect(html).toContain('tabindex="-1"');
  expect(html).toContain('Стороны</h2>');
  expect(html).toContain('aria-label="Клиент"');
});
