import { useLocalization } from '../localization/Localization';
import { useEffect, useState, type FormEvent } from 'react';
import { Select } from '../../shared/ui/Select';
import { currenciesWithCurrent } from '../finance/currencies';
import { useFieldValidation } from '../../shared/forms/fieldValidation';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';

export interface BankAccount {
  id: string;
  counterparty_id: string;
  name: string;
  number: string;
  currency: string;
  bank_name: string;
  bic?: string | null;
  swift?: string | null;
  correspondent_account?: string | null;
  iban?: string | null;
  is_primary: boolean;
  archived: boolean;
  revision: number;
}
const accountFields = [
  ['name', 'counterparty.accountName', 256],
  ['number', 'counterparty.accountNumber', 64],
  ['currency', 'registryField.currency', 3],
  ['bank_name', 'counterparty.bankName', 256],
  ['bic', 'counterparty.russianBankBIC', 9],
  ['swift', 'counterparty.foreignBankSWIFTBIC', 11],
  ['correspondent_account', 'counterparty.correspondentAccount', 64],
  ['iban', 'IBAN', 34],
] as const;

export function BankAccounts({
  counterpartyId,
  session,
  archivedOwner,
  version,
  onError,
  onDirty,
}: {
  counterpartyId: string;
  session: Session;
  archivedOwner: boolean;
  version: number;
  onError: (error: unknown) => void;
  onDirty: (dirty: boolean) => void;
}) {
  const { translate, locale } = useLocalization();
  const validation = useFieldValidation();
  const [items, setItems] = useState<BankAccount[]>([]);
  const [loading, setLoading] = useState(true);
  const [failure, setFailure] = useState('');
  const [refresh, setRefresh] = useState(0);
  const [showArchived, setShowArchived] = useState(false);
  const [editing, setEditing] = useState<BankAccount>();
  const [draft, setDraft] = useState<BankAccount>();
  const [busy, setBusy] = useState(false);
  const dirty = !!draft && JSON.stringify(draft) !== JSON.stringify(editing);
  const write =
    session.permissions.includes('counterparties.write_bank_accounts') && !archivedOwner;
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  useEffect(() => {
    onDirty(dirty || busy);
    return () => onDirty(false);
  }, [dirty, busy, onDirty]);
  useEffect(() => {
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    request<{ items: BankAccount[] }>(
      '/api/v1/counterparties/bank-accounts/list',
      session,
      { counterparty_id: counterpartyId },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) setItems(result.items);
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setFailure(error.message);
          onError(error);
        }
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [counterpartyId, session, version, refresh, onError]);
  function edit(value?: BankAccount) {
    const next = value ?? {
      id: crypto.randomUUID(),
      counterparty_id: counterpartyId,
      name: '',
      number: '',
      currency: 'RUB',
      bank_name: '',
      bic: null,
      swift: null,
      correspondent_account: null,
      iban: null,
      is_primary: false,
      archived: false,
      revision: 0,
    };
    validation.reset();
    setEditing(next);
    setDraft({ ...next });
    setFailure('');
  }
  function cancel() {
    if (!busy)
      confirmDiscard(dirty, () => {
        setDraft(undefined);
        setEditing(undefined);
        setFailure('');
      });
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!draft || busy) return;
    setBusy(true);
    setFailure('');
    try {
      const { revision, ...body } = draft;
      const account = Object.fromEntries(
        accountFields.map(([field]) => [
          field,
          ['name', 'number', 'currency', 'bank_name'].includes(field)
            ? (draft[field] ?? '').trim()
            : (draft[field] ?? '').trim() || null,
        ]),
      );
      await request('/api/v1/counterparties/bank-accounts/save', session, {
        ...body,
        ...account,
        expected_revision: revision,
      });
      setDraft(undefined);
      setEditing(undefined);
      setRefresh((value) => value + 1);
    } catch (error) {
      setFailure(
        error instanceof Error
          ? error.message
          : translate('counterparty.couldNotSaveTheBankAccount'),
      );
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <section className="bank-accounts">
      <div className="section-heading">
        <h2>{translate('counterparty.bankAccounts')}</h2>
        {write && !draft && (
          <button className="primary" onClick={() => edit()}>
            {translate('counterparty.addBankAccount')}
          </button>
        )}
      </div>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {draft ? (
        <form
          onSubmit={save}
          data-dirty={dirty}
          data-busy={busy}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
        >
          <h3>
            {draft.revision
              ? translate('counterparty.editBankAccount')
              : translate('counterparty.newBankAccount')}
          </h3>
          <fieldset disabled={busy} aria-label={translate('counterparty.bankAccountDetails')}>
            <div className="route-fields">
              {accountFields.map(([field, label, maxLength]) =>
                field === 'currency' ? (
                  <Select
                    key={field}
                    label={translate('registryField.currency')}
                    value={draft.currency}
                    searchable
                    options={currenciesWithCurrent(draft.currency, locale)}
                    disabled={busy}
                    onChange={(currency) => setDraft({ ...draft, currency })}
                  />
                ) : (
                  <label key={field}>
                    {label === 'IBAN' ? label : translate(label)}
                    <input
                      name={field}
                      value={draft[field] ?? ''}
                      maxLength={maxLength}
                      required={['name', 'number', 'bank_name'].includes(field)}
                      pattern={
                        field === 'bic'
                          ? '[0-9]{9}'
                          : field === 'swift'
                            ? '[A-Z0-9]{8}([A-Z0-9]{3})?'
                            : undefined
                      }
                      onChange={(event) =>
                        setDraft({
                          ...draft,
                          [field]:
                            field === 'swift'
                              ? event.target.value.toUpperCase()
                              : event.target.value,
                        })
                      }
                    />
                  </label>
                ),
              )}
            </div>
            <div className="role-options">
              <label>
                <input
                  type="checkbox"
                  checked={draft.is_primary}
                  disabled={draft.archived}
                  onChange={(event) => setDraft({ ...draft, is_primary: event.target.checked })}
                />
                {translate('counterparty.primaryForThisCurrency')}
              </label>
              <label>
                <input
                  type="checkbox"
                  checked={draft.archived}
                  onChange={(event) =>
                    setDraft({
                      ...draft,
                      archived: event.target.checked,
                      is_primary: event.target.checked ? false : draft.is_primary,
                    })
                  }
                />
                {translate('filter.archived')}
              </label>
            </div>
          </fieldset>
          <div className="form-actions">
            <button type="button" onClick={cancel} disabled={busy}>
              {translate('action.cancel')}
            </button>
            <button className="primary" disabled={busy}>
              {busy ? translate('shipment.saving') : translate('action.save')}
            </button>
          </div>
        </form>
      ) : (
        <>
          <label className="bank-archive-toggle">
            <input
              type="checkbox"
              checked={showArchived}
              onChange={(event) => setShowArchived(event.target.checked)}
            />
            {translate('finance.showArchived')}
          </label>
          {loading ? (
            <p className="hint">{translate('finance.loadingAccounts')}</p>
          ) : (
            <div className="bank-account-list">
              {items
                .filter((item) => showArchived || !item.archived)
                .map((item) => (
                  <article key={item.id} className="bank-account-card">
                    <div className="section-heading">
                      <div>
                        <h3>{item.name}</h3>
                        <p className="hint">
                          {item.currency}
                          {item.is_primary ? translate('counterparty.primary') : ''}
                          {item.archived ? translate('finance.archivedSuffix') : ''}
                        </p>
                      </div>
                      {write && (
                        <button onClick={() => edit(item)}>{translate('shipment.edit')}</button>
                      )}
                    </div>
                    <dl className="counterparty-details">
                      {accountFields
                        .filter(([field]) => !['name', 'currency'].includes(field))
                        .map(
                          ([field, label]) =>
                            item[field] && (
                              <div key={field}>
                                <dt>{label === 'IBAN' ? label : translate(label)}</dt>
                                <dd>{item[field]}</dd>
                              </div>
                            ),
                        )}
                    </dl>
                  </article>
                ))}
              {!items.some((item) => showArchived || !item.archived) && (
                <p className="hint">
                  {items.length
                    ? translate('counterparty.noActiveBankAccounts')
                    : translate('counterparty.noBankAccountsYet')}
                </p>
              )}
            </div>
          )}
        </>
      )}
      {confirmation}
    </section>
  );
}
