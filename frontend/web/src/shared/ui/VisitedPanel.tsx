import { useState, type ReactNode } from 'react';

/** Load a section on its first visit and retain its state until its card closes. */
export function VisitedPanel({
  active,
  children,
  className,
}: {
  active: boolean;
  children: ReactNode;
  className?: string;
}) {
  const [visited, setVisited] = useState(active);
  if (active && !visited) setVisited(true);
  return active || visited ? (
    <div hidden={!active} className={className}>
      {children}
    </div>
  ) : null;
}
