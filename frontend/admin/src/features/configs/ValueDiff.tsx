import { Fragment, useMemo, useState } from 'react';
import { changes } from './changes';
import { diffSections } from './diffSections';
import { lineDiff, type DiffCell, type DiffRow } from './lineDiff';

function Cell({ cell }: { cell?: DiffCell }) {
  return (
    <div className={'diff-cell ' + (cell?.kind ?? 'blank')}>
      <span className="line-number" aria-hidden="true">
        {cell?.line}
      </span>
      <span className="diff-marker" aria-hidden="true">
        {cell?.kind === 'added' ? '+' : cell?.kind === 'removed' ? '−' : ''}
      </span>
      <code>{cell?.text ?? ' '}</code>
    </div>
  );
}

function Rows({ rows }: { rows: DiffRow[] }) {
  return (
    <>
      {rows.map((row, index) => (
        <div className="diff-row" role="row" key={index}>
          <div role="cell">
            <Cell cell={row.before} />
          </div>
          <div role="cell">
            <Cell cell={row.after} />
          </div>
        </div>
      ))}
    </>
  );
}

function UnchangedRows({ rows }: { rows: DiffRow[] }) {
  const [expanded, setExpanded] = useState(false);
  return (
    <>
      <div className="diff-gap" role="row">
        <div role="cell" aria-colspan={2}>
          <button type="button" aria-expanded={expanded} onClick={() => setExpanded(!expanded)}>
            {expanded ? 'Свернуть' : 'Показать'} неизменённые строки · {rows.length}
          </button>
        </div>
      </div>
      {expanded && <Rows rows={rows} />}
    </>
  );
}

export function ValueDiff({ before, after }: { before: unknown; after: unknown }) {
  const count = useMemo(() => changes(before, after).length, [before, after]);
  const rows = useMemo(() => lineDiff(before, after), [before, after]);
  const sections = useMemo(() => diffSections(rows), [rows]);
  return (
    <section className="diff" aria-label="Изменения значения">
      <h3>Изменения · {count}</h3>
      {!count && <p className="muted">Значение не изменилось.</p>}
      <div className="split-diff" role="table" aria-label="Сравнение значений">
        <div className="diff-row diff-head" role="row">
          <div role="columnheader">Исходное значение</div>
          <div role="columnheader">Новое значение</div>
        </div>
        {sections.map((section) => (
          <Fragment key={`${section.start}:${section.end}`}>
            {section.collapsed ? (
              <UnchangedRows rows={rows.slice(section.start, section.end)} />
            ) : (
              <Rows rows={rows.slice(section.start, section.end)} />
            )}
          </Fragment>
        ))}
      </div>
    </section>
  );
}
