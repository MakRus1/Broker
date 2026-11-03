import { useLocalization } from '../localization/Localization';
import { useEffect, useRef, useState, type ReactNode } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import type { CounterpartyDetails } from './counterparties';
import { isInn, registryRequisites, type RegistryParty, type RegistryRequisites } from './registry';

export function RegistryLookup({
  session,
  draft,
  onApply,
  children,
}: {
  session: Session;
  draft: CounterpartyDetails;
  onApply: (value: RegistryRequisites) => void;
  children: (lookupButton: ReactNode) => ReactNode;
}) {
  const { translate } = useLocalization();
  const taxId = draft.tax_id.trim();
  const country = draft.country_code.trim().toUpperCase();
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [message, setMessage] = useState('');
  const pending = useRef<AbortController | undefined>(undefined);
  const requestNumber = useRef(0);
  useEffect(() => {
    ++requestNumber.current;
    pending.current?.abort();
    setFailure('');
    if (country && country !== 'RU') setMessage('');
    setBusy(false);
    return () => {
      ++requestNumber.current;
      pending.current?.abort();
    };
  }, [taxId, country]);
  useEffect(() => setMessage(''), [taxId]);
  async function lookup() {
    if (busy) return;
    if (!isInn(taxId)) {
      setFailure(translate('counterparty.checkTheINNEnter10Or12DigitsWithAValidChecksum'));
      return;
    }
    const controller = new AbortController();
    pending.current = controller;
    const number = ++requestNumber.current;
    setBusy(true);
    setFailure('');
    setMessage('');
    try {
      const result = await request<{ items: RegistryParty[] }>(
        '/api/v1/counterparties/lookup',
        session,
        { tax_id: taxId },
        controller.signal,
      );
      if (number !== requestNumber.current) return;
      if (result.items[0]) {
        onApply(registryRequisites(result.items[0]));
        setMessage(translate('counterparty.detailsFilledFromDaDataCheckThemBeforeSaving'));
      }
      if (!result.items.length) setMessage(translate('counterparty.lookupEmpty'));
    } catch (error) {
      if (!controller.signal.aborted && number === requestNumber.current)
        setFailure(
          error instanceof Error
            ? error.message
            : translate('counterparty.couldNotRetrieveCompanyDetails'),
        );
    } finally {
      if (number === requestNumber.current) setBusy(false);
    }
  }
  return (
    <section
      className="registry-lookup"
      aria-label={translate('counterparty.findCompanyDetailsByINN')}
      onKeyDown={(event) => {
        if (
          event.key !== 'Enter' ||
          event.nativeEvent.isComposing ||
          !(event.target instanceof HTMLInputElement) ||
          event.target.name !== 'tax_id'
        )
          return;
        event.preventDefault();
        if (taxId && (!country || country === 'RU')) void lookup();
      }}
    >
      {children(
        <button
          type="button"
          className="registry-search-button"
          title={translate('counterparty.findARussianHeadOfficeOrSoleProprietorInDaData')}
          disabled={busy || !taxId || (!!country && country !== 'RU')}
          onClick={() => void lookup()}
        >
          {busy ? translate('lookup.searching') : translate('counterparty.fillByINN')}
        </button>,
      )}
      {!!country && country !== 'RU' && (
        <p className="hint">{translate('counterparty.enterForeignCompanyDetailsManually')}</p>
      )}
      {message && <p role="status">{message}</p>}
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
    </section>
  );
}
