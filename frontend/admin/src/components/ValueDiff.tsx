import { changes } from '../changes';
import { lineDiff, type DiffCell } from '../lineDiff';

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

export function ValueDiff({ before, after }: { before: unknown; after: unknown }) {
  const count = changes(before, after).length;
  const rows = lineDiff(before, after);
  return (
    <section className="diff" aria-label="Изменения значения">
      <h3>Изменения · {count}</h3>
      {!count && <p className="muted">Значение не изменилось.</p>}
      <div className="split-diff" role="table" aria-label="Сравнение значений">
        <div className="diff-row diff-head" role="row">
          <div role="columnheader">Исходное значение</div>
          <div role="columnheader">Новое значение</div>
        </div>
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
      </div>
    </section>
  );
}
