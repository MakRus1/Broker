import './registryHeading.css';
import {
  RegistryColumnSettings,
  type RegistryColumns,
} from '../../features/registry/RegistryTable';
import { useLocalization } from '../../features/localization/Localization';
import type { ReactNode } from 'react';

/** Общие действия реестра находятся в шапке, отдельно от применённых фильтров. */
export function RegistryHeading({
  title,
  count,
  columns,
  children,
  onRefresh,
  loading = false,
}: {
  title?: string;
  count: number;
  columns: RegistryColumns;
  children?: ReactNode;
  onRefresh: () => void;
  loading?: boolean;
}) {
  const { translate } = useLocalization();
  return (
    <div className="page-heading registry-heading">
      <div>
        {title && <h1>{title}</h1>}
        <p className="hint" role="status">
          {loading ? translate('common.loading') : translate('common.loaded', { count })}
        </p>
      </div>
      <div className="actions">
        {children}
        <RegistryColumnSettings {...columns} />
        <button disabled={loading} onClick={onRefresh}>
          {translate('action.refresh')}
        </button>
      </div>
    </div>
  );
}
