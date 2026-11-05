import { useDelayedBusy } from './useDelayedBusy';
import { RegistrySavedViews } from '../../features/registry/RegistrySavedViews';
import './registryExperience.css';
import { RegistryGuide, RegistryTip } from './RegistryExperience';
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
  const showLoading = useDelayedBusy(loading);
  return (
    <div className="page-heading registry-heading">
      <div>
        {title && <h1>{title}</h1>}
        <p className="hint" role="status">
          {translate('common.loaded', { count })}
          <span className="registry-refresh-status" aria-hidden={!showLoading}>
            {showLoading ? translate('common.loading') : '\u00a0'}
          </span>
        </p>
      </div>
      <div className="actions">
        <div className="registry-heading-tools">
          <RegistrySavedViews state={columns} />
          <RegistryGuide />
          <RegistryColumnSettings {...columns} />
          <button
            aria-disabled={loading}
            onClick={() => {
              if (!loading) onRefresh();
            }}
          >
            {translate('action.refresh')}
          </button>
        </div>
        {children && <div className="registry-heading-create">{children}</div>}
      </div>
      <RegistryTip preference={columns.guidanceKey} />
    </div>
  );
}
