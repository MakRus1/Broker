import { useSyncExternalStore } from 'react';
import { useLocalization } from '../localization/Localization';
import { usePreferences } from './UserPreferences';
import type { PreferenceStore } from './preferenceStore';

export function PreferenceNotice() {
  const store = usePreferences();
  return store ? <StoreNotice store={store} /> : null;
}
function StoreNotice({ store }: { store: PreferenceStore }) {
  const { translate } = useLocalization();
  useSyncExternalStore(store.subscribe, store.snapshot);
  return store.error ? (
    <div className="preference-notice" role="status">
      {translate(store.error)}
      <button onClick={() => void store.retry()}>{translate('filter.retry')}</button>
    </div>
  ) : null;
}
