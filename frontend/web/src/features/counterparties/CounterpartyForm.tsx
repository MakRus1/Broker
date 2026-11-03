import { saveErrorMessage } from '../../shared/forms/saveError';
import { MultiSelect } from '../../shared/ui/MultiSelect';
import './counterpartyEditor.css';
import { useFieldValidation } from '../../shared/forms/fieldValidation';
import { RecordHeading, RecordSection, RecordActions } from '../../shared/ui/record/RecordLayout';
import { registryValueLabel } from '../registry/registryLabels';
import { useLocalization } from '../localization/Localization';
import { useEffect, useState, type FormEvent } from 'react';
import { DirectoryLookup } from '../directories/DirectoryLookup';
import { RegistryLookup } from './RegistryLookup';
import { ApiError, request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';
import {
  counterpartyDetails,
  validateContacts,
  counterpartyPayload,
  counterpartyRoles,
  restoreCounterpartyDraft,
  type Counterparty,
  type CounterpartyDetails,
  type CounterpartyRole,
} from './counterparties';

export function CounterpartyForm({
  initial,
  session,
  onSaved,
  onError,
  onCancel,
}: {
  initial?: Counterparty;
  onCancel?: () => void;
  session: Session;
  onSaved: (value: Counterparty) => void;
  onError: (value: unknown) => void;
}) {
  const { translate, locale } = useLocalization();
  const validation = useFieldValidation();
  const storageKey = `broker:counterparty-draft:${session.actor}:${initial?.id ?? 'new'}`;
  const [stored] = useState(() => {
    const fallback = {
      id: initial?.id ?? crypto.randomUUID(),
      baseRevision: initial?.revision ?? 0,
      value: counterpartyDetails(initial),
    };
    try {
      return restoreCounterpartyDraft(sessionStorage.getItem(storageKey), fallback);
    } catch {
      return fallback;
    }
  });
  const [draft, setDraft] = useState(stored.value);
  const id = stored.id;
  const [busy, setBusy] = useState(false);
  const [saved, setSaved] = useState(false);
  const [failure, setFailure] = useState('');
  const dirty =
    !saved &&
    JSON.stringify(counterpartyPayload(draft)) !==
      JSON.stringify(counterpartyPayload(counterpartyDetails(initial)));
  const stale = stored.baseRevision !== (initial?.revision ?? 0) || (initial && id !== initial.id);
  useEffect(() => {
    try {
      if (dirty) sessionStorage.setItem(storageKey, JSON.stringify({ ...stored, value: draft }));
      else sessionStorage.removeItem(storageKey);
    } catch {
      /* Недоступное локальное хранилище не блокирует редактирование. */
    }
  }, [storageKey, stored, draft, dirty]);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  useEffect(() => {
    function leaving(event: BeforeUnloadEvent) {
      if (dirty || busy) {
        event.preventDefault();
        event.returnValue = '';
      }
    }
    window.addEventListener('beforeunload', leaving);
    return () => window.removeEventListener('beforeunload', leaving);
  }, [dirty, busy]);
  function set<K extends keyof CounterpartyDetails>(key: K, value: CounterpartyDetails[K]) {
    setDraft((old) => ({ ...old, [key]: value }));
  }
  function cancel() {
    if (!busy)
      confirmDiscard(dirty, () => {
        try {
          sessionStorage.removeItem(storageKey);
        } catch {
          /* Отмена доступна без хранилища. */
        }
        onCancel?.();
      });
  }
  async function submit(event: FormEvent) {
    event.preventDefault();
    if (busy || stale) return;
    setBusy(true);
    setFailure('');
    try {
      validateContacts(draft.contacts, locale);
      const result = await request<{ counterparty: Counterparty }>(
        `/api/v1/counterparties/${initial ? 'update' : 'create'}`,
        session,
        {
          id,
          ...counterpartyPayload(draft),
          ...(initial ? { expected_revision: initial.revision } : {}),
        },
      );
      try {
        sessionStorage.removeItem(storageKey);
      } catch {
        /* Запись на сервер не зависит от хранилища. */
      }
      setSaved(true);
      onSaved(result.counterparty);
    } catch (error) {
      setFailure(
        error instanceof ApiError && error.status === 409 && error.code !== 'revision_conflict'
          ? error.message +
              translate('counterparty.checkTheExistingRecordOrRefreshTheDataBeforeRetrying')
          : saveErrorMessage(error, translate('counterparty.couldNotSave')),
      );
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <>
      <form
        className="counterparty-form document-editor"
        data-draft-key={storageKey}
        data-dirty={dirty}
        data-busy={busy}
        onSubmit={submit}
        data-validation-attempted={validation.attempted}
        onInvalid={validation.onInvalid}
        onInput={validation.onInput}
      >
        <RecordHeading
          guide="counterparty"
          kind={translate('nav.counterparties')}
          title={initial ? initial.name : translate('entity.newCounterparty')}
          mode={translate(initial ? 'counterparty.editCounterparty' : 'record.creating')}
        />
        {stale && (
          <p className="error" role="alert">
            {translate('counterparty.draftConflict')}
          </p>
        )}
        <fieldset disabled={busy || !!stale}>
          <RecordSection title={translate('counterparty.legalEntity')}>
            <label className="record-toggle">
              <input
                type="checkbox"
                role="switch"
                checked={draft.own_company}
                onChange={(event) => set('own_company', event.target.checked)}
              />
              {translate('counterparty.ownLegalEntity')}
            </label>
            <RegistryLookup
              session={session}
              draft={draft}
              onApply={(value) => setDraft((old) => ({ ...old, ...value }))}
            >
              {(lookupButton) => (
                <div className="counterparty-requisites">
                  {(
                    [
                      ['name', translate('counterparty.shortNameRequired'), 256],
                      ['legal_name', translate('counterparty.legalName'), 512],
                      ['tax_id', translate('counterparty.taxIDINN'), 64],
                      ['country_code', translate('counterparty.countryCodeRUCN'), 2],
                      ['kpp', translate('counterparty.kpp'), 9],
                      ['registration_id', translate('counterparty.registrationNumberOGRN'), 64],
                      ['legal_address', translate('counterparty.registeredAddress'), 1024],
                    ] as const
                  ).map(([key, label, limit]) => (
                    <div
                      key={key}
                      className={`counterparty-field-${key}${key === 'tax_id' ? ' registry-tax-field' : ''}`}
                    >
                      <label>
                        {label}
                        {key === 'legal_name' || key === 'legal_address' ? (
                          <textarea
                            rows={1}
                            value={draft[key]}
                            maxLength={limit}
                            onChange={(event) => set(key, event.target.value)}
                          />
                        ) : (
                          <input
                            name={key}
                            value={draft[key]}
                            required={key === 'name' || (key === 'country_code' && !!draft.tax_id)}
                            maxLength={limit}
                            pattern={
                              key === 'kpp'
                                ? '[0-9]{9}'
                                : key === 'country_code'
                                  ? '[A-Za-z]{2}'
                                  : key === 'tax_id'
                                    ? '[A-Za-z0-9 -]*'
                                    : undefined
                            }
                            onChange={(event) => set(key, event.target.value)}
                          />
                        )}
                      </label>
                      {key === 'tax_id' && lookupButton}
                    </div>
                  ))}
                </div>
              )}
            </RegistryLookup>
          </RecordSection>
        </fieldset>
        <fieldset disabled={busy}>
          <RecordSection title={translate('registryField.roles')}>
            <p className="hint">{translate('counterparty.rolesHint')}</p>
            <MultiSelect
              label={translate('counterparty.selectRoles')}
              values={draft.roles}
              disabled={busy}
              options={Object.entries(counterpartyRoles).map(([value, label]) => ({
                value,
                label: registryValueLabel('roles', value, locale) ?? label,
              }))}
              onChange={(roles) => set('roles', roles as CounterpartyRole[])}
            />
          </RecordSection>
        </fieldset>
        <fieldset disabled={busy}>
          <RecordSection title={translate('counterparty.responsibleEmployee')}>
            <DirectoryLookup
              session={session}
              kind="employees"
              label={translate('shipment.logist')}
              value={draft.logist}
              onChange={(value) => set('logist', value)}
            />
            <p className="hint">{translate('counterparty.logistHint')}</p>
          </RecordSection>
        </fieldset>
        <fieldset disabled={busy}>
          <RecordSection title={translate('counterparty.contacts')}>
            {draft.contacts.map((contact, index) => (
              <div className="contact-editor" key={index}>
                <div className="route-fields">
                  {(['name', 'position', 'email', 'phone'] as const).map((key) => (
                    <label key={key}>
                      {
                        {
                          name: translate('counterparty.nameRequired'),
                          position: translate('counterparty.position'),
                          email: 'Email',
                          phone: translate('counterparty.phone'),
                        }[key]
                      }
                      <input
                        required={key === 'name'}
                        type={key === 'email' ? 'email' : key === 'phone' ? 'tel' : 'text'}
                        title={
                          key === 'phone'
                            ? translate(
                                'counterparty.7To15DigitsSpacesParenthesesAndHyphensAreAllowed',
                              )
                            : undefined
                        }
                        maxLength={key === 'phone' ? 64 : 256}
                        value={contact[key]}
                        onChange={(event) =>
                          set(
                            'contacts',
                            draft.contacts.map((value, i) =>
                              i === index ? { ...value, [key]: event.target.value } : value,
                            ),
                          )
                        }
                      />
                    </label>
                  ))}
                </div>
                <button
                  type="button"
                  onClick={() =>
                    set(
                      'contacts',
                      draft.contacts.filter((_, i) => i !== index),
                    )
                  }
                >
                  {translate('counterparty.removeContact')}
                </button>
              </div>
            ))}
            <button
              type="button"
              disabled={draft.contacts.length >= 32}
              onClick={() =>
                set('contacts', [
                  ...draft.contacts,
                  { name: '', position: '', email: '', phone: '' },
                ])
              }
            >
              {translate('counterparty.addContact')}
            </button>
          </RecordSection>
        </fieldset>
        <RecordActions
          error={failure || validation.message}
          status={
            dirty
              ? translate('shipment.unsaved')
              : translate('counterparty.theDraftIsSavedInThisTab')
          }
        >
          <button type="button" disabled={busy} onClick={cancel}>
            {translate('action.cancel')}
          </button>
          <button className="primary" disabled={busy || !!stale || !dirty || !draft.name.trim()}>
            {busy
              ? translate('shipment.saving')
              : initial
                ? translate('shipment.saveChanges')
                : translate('counterparty.createCounterparty')}
          </button>
        </RecordActions>
      </form>
      {confirmation}
    </>
  );
}
