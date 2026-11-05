import { useEffect, useState } from 'react';

/** Fast requests should not flash a loading label between two stable states. */
export function useDelayedBusy(busy: boolean, delay = 250) {
  const [visible, setVisible] = useState(false);
  useEffect(() => {
    if (!busy) {
      setVisible(false);
      return;
    }
    const timer = setTimeout(() => setVisible(true), delay);
    return () => clearTimeout(timer);
  }, [busy, delay]);
  return busy && visible;
}
