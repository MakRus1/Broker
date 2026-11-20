import type { ComponentPropsWithRef, ReactNode } from 'react';

/** Шапка, дополнительные инструменты и содержимое всегда занимают одни и те же места. */
export function RegistryPage({
  className = '',
  heading,
  tools,
  children,
  ...props
}: ComponentPropsWithRef<'section'> & { heading?: ReactNode; tools?: ReactNode }) {
  return (
    <section {...props} className={`registry-page registry-workbench ${className}`}>
      {heading}
      {tools && <div className="registry-page-tools">{tools}</div>}
      <div className="registry-page-content">{children}</div>
    </section>
  );
}
