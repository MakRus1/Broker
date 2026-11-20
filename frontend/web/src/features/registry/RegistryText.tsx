import { useState, type ReactNode } from 'react';
import { useLocalization } from '../localization/Localization';

/** Длинное значение не должно задавать высоту всего реестра. */
export function RegistryText({ value }: { value: ReactNode }) {
  const [expanded, setExpanded] = useState(false);
  const { translate } = useLocalization();
  if (typeof value !== 'string' || value.length <= 120) return value;
  return (
    <div className="registry-long-text" data-expanded={expanded}>
      <span>{value}</span>
      <button
        type="button"
        aria-expanded={expanded}
        onClick={(event) => {
          event.stopPropagation();
          setExpanded(!expanded);
        }}
      >
        {translate(expanded ? 'registry.collapseText' : 'registry.expandText')}
      </button>
    </div>
  );
}
