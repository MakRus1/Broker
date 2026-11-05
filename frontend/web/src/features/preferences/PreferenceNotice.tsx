import { useNotifications } from '../notifications/Notifications';
import { useEffect, useSyncExternalStore } from 'react';
import { useLocalization } from '../localization/Localization';
import { usePreferences } from './UserPreferences';
import type { PreferenceStore } from './preferenceStore';

export function PreferenceNotice() {
  const store = usePreferences();
  return store ? <StoreNotice store={store} /> : null;
}
function StoreNotice({ store }: { store: PreferenceStore }) {
  const { translate } = useLocalization();
  const notifications = useNotifications();
  const version = useSyncExternalStore(store.subscribe, store.snapshot);
  useEffect(() => {
    if (store.error)
      notifications.publish({
        id: 'preferences',
        message: translate(store.error),
        level: 'warning',
        action: { label: translate('filter.retry'), run: () => store.retry() },
      });
    else notifications.resolve('preferences');
  }, [store, version, translate, notifications]);
  return null;
}
