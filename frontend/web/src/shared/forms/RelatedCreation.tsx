import { createContext, useContext, useEffect, useRef, type ReactNode } from 'react';
import { useLocalization } from '../../features/localization/Localization';
import type { Counterparty } from '../../features/counterparties/counterparties';
import type { Contract } from '../../features/contracts/ContractsPage';
import type { Invoice, InvoiceDirection } from '../../features/finance/invoices/invoices';
import type { BankAccount } from '../../features/counterparties/BankAccounts';

import type { FinancialLabel } from '../../features/finance/labels/financialLabels';

export type RelatedDefaults = {
  financialLabel: Record<string, never>;
  counterparty: { ownCompany?: boolean };
  contract: { counterparty_id?: string; own_company_id?: string; currency?: string };
  bankAccount: { owner: string; currency: string };
  invoice: {
    direction: InvoiceDirection;
    counterparty_id: string;
    counterparty_name: string;
    contract_id: string | null;
    contract_number: string | null;
    currency: string;
  };
};
type RelatedModels = {
  financialLabel: FinancialLabel;
  counterparty: Counterparty;
  contract: Contract;
  bankAccount: BankAccount;
  invoice: Invoice;
};
export type RelatedKind = keyof RelatedDefaults;
export type RelatedRequest = {
  [K in RelatedKind]: {
    kind: K;
    defaults: RelatedDefaults[K];
  };
}[RelatedKind];
export function relatedCreationRoute(
  kind: RelatedKind,
  id: string,
  direction: InvoiceDirection = 'outgoing',
) {
  const section = {
    financialLabel: 'financial-labels',
    counterparty: 'counterparties',
    contract: 'contracts',
    bankAccount: 'bank-accounts',
    invoice: `${direction}-invoices`,
  }[kind];
  return `/${section}/new/${id}`;
}
type Pending = RelatedRequest & {
  origin: string;
  originTitle: string;
  select: (value: unknown) => void;
};
export class CreationRequests {
  private readonly suspendedDialogs = new Map<HTMLDialogElement, string>();
  readonly pending = new Map<string, Pending>();
  start<K extends RelatedKind>(
    kind: K,
    defaults: RelatedDefaults[K],
    select: (value: RelatedModels[K]) => void,
  ) {
    const route = relatedCreationRoute(
      kind,
      crypto.randomUUID(),
      'direction' in defaults ? defaults.direction : undefined,
    );
    const dialogs = [...document.querySelectorAll<HTMLDialogElement>('dialog[open]')];
    const origin = location.hash.slice(1) || '/';
    for (const dialog of dialogs) {
      this.suspendedDialogs.set(dialog, origin);
      dialog.close();
    }
    this.pending.set(route, {
      kind,
      defaults,
      origin,
      originTitle: document.title.replace(/ · Broker$/, ''),
      select: select as (value: unknown) => void,
    } as Pending);
    location.hash = '#' + route;
    return route;
  }
  finish(route: string, value: unknown) {
    const request = this.pending.get(route);
    if (!request) return;
    this.pending.delete(route);
    request.select(value);
    return request.origin;
  }
  cancel(route: string) {
    const request = this.pending.get(route);
    if (!request) return;
    this.pending.delete(route);
    return request.origin;
  }
  restoreDialogs(route: string) {
    for (const [dialog, origin] of this.suspendedDialogs) {
      if (!dialog.isConnected) {
        this.suspendedDialogs.delete(dialog);
        continue;
      }
      if (route === origin) {
        if (!dialog.open) dialog.showModal();
        this.suspendedDialogs.delete(dialog);
      }
    }
  }
  clear() {
    this.pending.clear();
    this.suspendedDialogs.clear();
  }
}
const RelatedCreation = createContext<CreationRequests | null>(null);
export function RelatedCreationProvider({ children }: { children: ReactNode }) {
  const requests = useRef(new CreationRequests()).current;
  useEffect(() => {
    const restore = () => requests.restoreDialogs(location.hash.slice(1) || '/');
    window.addEventListener('hashchange', restore);
    return () => window.removeEventListener('hashchange', restore);
  }, [requests]);
  return <RelatedCreation.Provider value={requests}>{children}</RelatedCreation.Provider>;
}
export function useRelatedRequests() {
  return useContext(RelatedCreation);
}

/** Completion is scoped to this mounted field and its current owner/filter context. */
export function useRelatedCreate<K extends RelatedKind>({
  kind,
  defaults,
  scope,
  enabled,
  onCreated,
}: {
  kind: K;
  defaults: RelatedDefaults[K];
  scope: string;
  enabled: boolean;
  onCreated: (value: RelatedModels[K]) => void;
}) {
  const requests = useRelatedRequests();
  const current = useRef({ scope, enabled, onCreated });
  current.current = { scope, enabled, onCreated };
  const routes = useRef<string[]>([]);
  useEffect(
    () => () => {
      // The source form may close while the new record remains open. Never update an unmounted field.
      for (const route of routes.current) requests?.pending.delete(route);
    },
    [requests],
  );
  return requests && enabled
    ? () => {
        routes.current.push(
          requests.start(kind, defaults, (value) => {
            if (current.current.enabled && current.current.scope === scope)
              current.current.onCreated(value);
          }),
        );
      }
    : undefined;
}

const returnHint = {
  financialLabel: 'related.labelReturnHint',
  counterparty: 'related.counterpartyReturnHint',
  contract: 'related.contractReturnHint',
  bankAccount: 'related.accountReturnHint',
  invoice: 'related.invoiceReturnHint',
} as const;
export function RelatedCreationHint({ route }: { route: string }) {
  const request = useRelatedRequests()?.pending.get(route);
  const { translate } = useLocalization();
  return request ? (
    <p className="related-creation-hint">
      {translate(returnHint[request.kind], { title: request.originTitle })}
    </p>
  ) : null;
}
