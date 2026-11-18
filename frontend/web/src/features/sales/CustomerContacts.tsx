import { useEffect, useState } from 'react';
import './customerContacts.css';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { Select } from '../../shared/ui/Select';
import { CounterpartyContacts } from '../counterparties/CounterpartyContacts';
import type { Counterparty } from '../counterparties/counterparties';
import { useLocalization } from '../localization/Localization';
import { useRegistryFreshness } from '../registry/useRegistryFreshness';

type Props = { customerId: string; session: Session; active: boolean; contactId?: string };
function useCustomerContacts({ customerId, session, active }: Props) {
  const freshness = useRegistryFreshness([session, customerId]);
  const [loaded, setLoaded] = useState<{ id: string; session: Session; customer: Counterparty }>();
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  useEffect(() => {
    if (!active || !customerId || freshness.isFresh()) return;
    const controller = new AbortController();
    setFailure('');
    request<{ counterparty: Counterparty }>(
      '/api/v1/counterparties/get',
      session,
      { id: customerId },
      controller.signal,
    )
      .then(({ counterparty }) => {
        if (controller.signal.aborted) return;
        setLoaded({ id: customerId, session, customer: counterparty });
        freshness.complete();
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [customerId, session, active, version, freshness.version]);
  return {
    customer: loaded?.id === customerId && loaded.session === session ? loaded.customer : undefined,
    failure,
    retry: () => {
      freshness.clear();
      setVersion((v) => v + 1);
    },
  };
}

export function CustomerContacts(props: Props) {
  const { translate } = useLocalization();
  const { customer, failure, retry } = useCustomerContacts(props);
  return (
    <section className="sales-customer-contacts">
      <div className="sales-customer-contacts-heading">
        <div className="record-layout-section-heading">
          <h2>{translate('sales.contacts')}</h2>
          <p>
            {translate('sales.customer')}:{' '}
            {customer ? (
              <a className="link" href={`#/counterparties/${customer.id}`}>
                {customer.name}
              </a>
            ) : (
              translate('common.loading')
            )}
          </p>
          <p>{translate('sales.contactsFromClient')}</p>
        </div>
        {props.session.permissions.includes('counterparties.write') && (
          <div className="record-layout-actions">
            <a className="button" href={`#/counterparties/${props.customerId}/edit`}>
              {translate('sales.manageContacts')}
            </a>
          </div>
        )}
      </div>
      {failure ? (
        <div role="alert">
          <p className="error">{failure}</p>
          <button onClick={retry}>{translate('ui.retryLoad')}</button>
        </div>
      ) : customer ? (
        <>
          {props.contactId &&
            !customer.contacts.some((contact) => contact.id === props.contactId) && (
              <p className="hint">{translate('sales.contactUnavailable')}</p>
            )}
          <CounterpartyContacts contacts={customer.contacts} selectedId={props.contactId} />
        </>
      ) : (
        <p role="status">{translate('common.loading')}</p>
      )}
    </section>
  );
}

export function CustomerContactPerson(props: Props & { onChange?: (id: string) => void }) {
  const { translate } = useLocalization();
  const { customer, failure, retry } = useCustomerContacts({
    ...props,
    active: props.active && (!!props.onChange || !!props.contactId),
  });
  const selected = customer?.contacts.find((contact) => contact.id === props.contactId);
  if (!props.onChange && !props.contactId) return <span>—</span>;
  if (failure)
    return (
      <div role="alert">
        <p className="error">{failure}</p>
        <button type="button" onClick={retry}>
          {translate('ui.retryLoad')}
        </button>
      </div>
    );
  return (
    <div>
      {props.onChange ? (
        <Select
          label={translate('sales.contactPerson')}
          value={props.contactId ?? ''}
          selectedLabel={!customer && props.contactId ? translate('common.loading') : undefined}
          disabled={!customer}
          onChange={props.onChange}
          options={[
            { value: '', label: translate('ui.notSelected') },
            ...(customer?.contacts
              .filter((contact) => contact.id)
              .map((contact) => ({
                value: contact.id!,
                label: contact.position ? `${contact.name} · ${contact.position}` : contact.name,
              })) ?? []),
            ...(customer && props.contactId && !selected
              ? [{ value: props.contactId, label: translate('sales.contactUnavailable') }]
              : []),
          ]}
        />
      ) : !customer ? (
        <span>{translate('common.loading')}</span>
      ) : selected ? (
        <strong>{selected.name}</strong>
      ) : (
        <span className="hint">{translate('sales.contactUnavailable')}</span>
      )}
      {props.onChange && props.session.permissions.includes('counterparties.write') && (
        <p className="hint">
          <a href={`#/counterparties/${props.customerId}/edit`}>
            {translate('sales.manageContacts')}
          </a>
        </p>
      )}
      {selected && (
        <div className="hint">
          {!props.onChange && selected.position && <span>{selected.position} · </span>}
          {selected.email && (
            <a href={`mailto:${encodeURIComponent(selected.email)}`}>{selected.email}</a>
          )}
          {selected.email && selected.phone && ' · '}
          {selected.phone && (
            <a href={`tel:${selected.phone.replace(/[^\d+]/g, '')}`}>{selected.phone}</a>
          )}
        </div>
      )}
    </div>
  );
}
