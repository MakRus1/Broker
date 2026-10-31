import type { ReactNode } from 'react';
export function RegistrySortHeading<T extends string>({
  field,
  sort,
  descending,
  onSort,
  children,
}: {
  field: T;
  sort: T;
  descending: boolean;
  onSort: (field: T) => void;
  children: ReactNode;
}) {
  return (
    <th aria-sort={sort === field ? (descending ? 'descending' : 'ascending') : undefined}>
      <button className="table-sort" onClick={() => onSort(field)}>
        {children} {sort === field ? (descending ? '↓' : '↑') : '↕'}
      </button>
    </th>
  );
}
