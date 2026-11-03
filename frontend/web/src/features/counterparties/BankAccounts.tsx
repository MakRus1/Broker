import { RecordEditorSections } from '../../shared/ui/record/RecordEditorSections';
import { draftStorageKey, useDraftRecovery } from '../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../shared/ui/record/DraftRecovery';
import { saveErrorMessage } from '../../shared/forms/saveError';
import { RecordSection, RecordActions } from '../../shared/ui/record/RecordLayout';
import './bankAccounts.css';
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
  ['name', 'counterparty.accountName', 256, 'account'],
  ['number', 'counterparty.accountNumber', 64, 'account'],
  ['currency', 'registryField.currency', 3, 'account'],
  ['bank_name', 'counterparty.bankName', 256, 'bank'],
  ['bic', 'counterparty.russianBankBIC', 9, 'bank'],
  ['swift', 'counterparty.foreignBankSWIFTBIC', 11, 'bank'],
  ['correspondent_account', 'counterparty.correspondentAccount', 64, 'bank'],
  ['iban', 'IBAN', 34, 'bank'],
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
  const storageKey = draftStorageKey(session, 'bank-account', counterpartyId);
  const recovery = useDraftRecovery({
    storageKey,
    revision: draft?.revision ?? 0,
    value: { draft, baseline: editing },
    dirty,
    enabled: write && !!draft && !loading,
    revisionForDraft(raw) {
      const next = raw as { draft?: BankAccount };
      if (!next?.draft) return -1;
      return next.draft.revision === 0
        ? 0
        : (items.find((x) => x.id === next.draft?.id)?.revision ?? -1);
    },
    onRestore(raw) {
      const next = raw as { draft: BankAccount; baseline: BankAccount };
      if (
        !next?.draft ||
        next.draft.counterparty_id !== counterpartyId ||
        typeof next.draft.number !== 'string' ||
        !next.baseline
      )
        throw new Error('Invalid draft');
      setDraft(next.draft);
      setEditing(next.baseline);
    },
  });
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
        recovery.discard();
        setDraft(undefined);
        setEditing(undefined);
        setFailure('');
      });
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!draft || busy || recovery.candidate) return;
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
      recovery.markSaved();
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
      {write && !loading && <DraftRecovery recovery={recovery} />}
      <div className="section-heading">
        <h2>
          {draft
            ? translate(
                draft.revision ? 'counterparty.editBankAccount' : 'counterparty.newBankAccount',
              )
            : translate('counterparty.bankAccounts')}
        </h2>
        {!draft && (
          <div className="bank-accounts-actions">
            <label className="bank-archive-toggle">
              <input
                type="checkbox"
                checked={showArchived}
                onChange={(event) => setShowArchived(event.target.checked)}
              />
              {translate('finance.showArchived')}
            </label>
            {write && (
              <button className="primary" onClick={() => edit()}>
                {translate('counterparty.addBankAccount')}
              </button>
            )}
          </div>
        )}
      </div>
      {failure && !draft && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {draft ? (
        <form
          data-recovery-key={storageKey}
          className="bank-account-editor"
          onSubmit={save}
          data-dirty={dirty}
          data-busy={busy}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
        >
          <RecordEditorSections>
            <fieldset
              className="record-layout-content"
              disabled={busy || !!recovery.candidate}
              aria-label={translate('counterparty.bankAccountDetails')}
            >
              {(
                [
                  ['account', 'counterparty.bankAccountDetails'],
                  ['bank', 'print.bankDetails'],
                ] as const
              ).map(([group, title]) => (
                <RecordSection key={group} title={translate(title)}>
                  <div className="record-fields">
                    {accountFields
                      .filter((field) => field[3] === group)
                      .map(([field, label, maxLength]) =>
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
                </RecordSection>
              ))}
              <RecordSection title={translate('record.additional')}>
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
              </RecordSection>
            </fieldset>
          </RecordEditorSections>
          <RecordActions
            error={failure || validation.message}
            status={
              dirty
                ? translate(
                    recovery.persisted && !recovery.failed ? 'draft.local' : 'shipment.unsaved',
                  )
                : draft.revision
                  ? translate('shipment.revision', { revision: draft.revision })
                  : translate('record.creating')
            }
          >
            <button type="button" onClick={cancel} disabled={busy}>
              {translate('action.cancel')}
            </button>
            <button className="primary" disabled={busy || !!recovery.candidate}>
              {busy ? translate('shipment.saving') : translate('action.save')}
            </button>
          </RecordActions>
        </form>
      ) : (
        <>
          {loading ? (
            <p className="hint">{translate('finance.loadingAccounts')}</p>
          ) : (
            <div className="bank-account-list">
              {items
                .filter((item) => showArchived || !item.archived)
                .map((item) => (
                  <BankAccountCard
                    key={item.id}
                    account={item}
                    onEdit={write ? () => edit(item) : undefined}
                  />
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

export function BankAccountCard({
  account,
  onEdit,
}: {
  account: BankAccount;
  onEdit?: () => void;
}) {
  const { translate } = useLocalization();
  return (
    <details className="bank-account-card">
      <summary className="bank-account-summary">
        <span className="bank-account-identity">
          <strong>{account.name}</strong>
          <span className="bank-account-number">{account.number}</span>
        </span>
        <span className="bank-account-bank">{account.bank_name}</span>
        <span className="bank-account-status">
          <span>{account.currency}</span>
          {account.is_primary && (
            <span className="badge">{translate('counterparty.primaryAccount')}</span>
          )}
          {account.archived && <span className="badge">{translate('filter.archived')}</span>}
        </span>
      </summary>
      <div className="bank-account-body">
        <div className="bank-account-body-heading">
          <span>{translate('shipment.revision', { revision: account.revision })}</span>
          {onEdit && (
            <button type="button" onClick={onEdit}>
              {translate('shipment.edit')}
            </button>
          )}
        </div>
        <dl className="record-layout-details">
          {accountFields
            .filter(([field]) => !['name', 'currency'].includes(field))
            .map(
              ([field, label]) =>
                account[field] && (
                  <div key={field}>
                    <dt>{label === 'IBAN' ? label : translate(label)}</dt>
                    <dd>{account[field]}</dd>
                  </div>
                ),
            )}
        </dl>
      </div>
    </details>
  );
}
