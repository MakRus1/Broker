import { RelatedCreationHint, useRelatedRequests } from '../../shared/forms/RelatedCreation';
import { RecordHeading } from '../../shared/ui/record/RecordLayout';
import { RegistryResultState } from '../../shared/ui/RegistryExperience';
import { registryValueLabel } from '../registry/registryLabels';
import { RegistryHeading } from '../../shared/ui/RegistryHeading';
import { useLocalization } from '../localization/Localization';
import { CounterpartySettlements } from '../finance/settlements/CounterpartySettlements';
import { BankAccounts } from './BankAccounts';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import {
  initialCounterpartyQuery,
  restoreCounterpartyQuery,
} from '../registry/registryQueryPreferences';
import { useColumnFilters } from '../registry/RegistryColumnFilters';
import { filterRequest } from '../registry/registryFilters';
import { RegistryTable, useRegistryColumns, type RegistryColumn } from '../registry/RegistryTable';
import type { CounterpartyRegistryCursor, CounterpartySort } from './counterpartyRegistry';
import { EmployeeName } from '../../shared/profile/EmployeeName';
import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { useCallback, useEffect, useRef, useState } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { CardNavigation } from '../../shared/ui/CardNavigation';
import { Dialog } from '../../shared/ui/Dialog';
import { CounterpartyHistory } from './CounterpartyHistory';
import { counterpartyRoles, type Counterparty } from './counterparties';
import { CounterpartyForm } from './CounterpartyForm';

type Props = {
  route: string;
  onTitle?: (title: string) => void;
  active?: boolean;
  session: Session;
  onError: (error: unknown) => void;
  onChanged: (value: Counterparty) => void;
  onSaved: (value: Counterparty) => void;
  onCancel: () => void;
};
export function CounterpartiesPage({
  route,
  session,
  onError,
  onChanged,
  onSaved,
  onCancel,
  onTitle,
  active = true,
}: Props) {
  const { translate, locale, catalogVersion } = useLocalization();
  const columns: RegistryColumn<Counterparty>[] = [
    {
      id: 'name',
      labelKey: 'registryField.name',
      width: 340,
      required: true,
      sort: 'name',
      render: (value) => (
        <a
          className="link"
          href={`#/counterparties/${value.id}`}
          onClick={(event) => event.stopPropagation()}
        >
          {value.name}
        </a>
      ),
    },
    {
      id: 'country_code',
      labelKey: 'registryField.country_code',
      width: 150,
      sort: 'country_code',
      render: (value) => value.country_code || '—',
    },
    {
      id: 'tax_id',
      labelKey: 'registryField.tax_id',
      width: 210,
      sort: 'tax_id',
      render: (value) => value.tax_id || '—',
    },
    {
      id: 'roles',
      labelKey: 'registryField.roles',
      width: 280,
      render: (value) =>
        value.roles.map((role) => registryValueLabel('roles', role, locale)).join(', ') || '—',
    },
    {
      id: 'own_company',
      labelKey: 'registryField.own_company',
      width: 180,
      render: (value) => (value.own_company ? translate('filter.yes') : translate('filter.no')),
    },
    {
      id: 'archived',
      labelKey: 'registryField.archived',
      width: 180,
      sort: 'archived',
      render: (value) =>
        value.archived ? translate('filter.archived') : translate('filter.active'),
    },
  ];
  const columnState = useRegistryColumns(session.subject, 'counterparties', columns);
  const id = /^\/counterparties\/([a-f0-9-]{36})(?:\/edit)?$/i.exec(route)?.[1];
  const creating = /^\/counterparties\/new(?:\/|$)/.test(route);
  const related = useRelatedRequests()?.pending.get(route);
  const creationId = creating ? route.split('/')[3] : undefined;
  const editing = creating || route.endsWith('/edit');
  const [tab, setTab] = useState<'details' | 'history' | 'accounts' | 'settlements'>('details');
  const [accountsDirty, setAccountsDirty] = useState(false);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  function switchTab(next: 'details' | 'history' | 'accounts' | 'settlements') {
    confirmDiscard(accountsDirty, () => setTab(next));
  }
  const [historyVersion, setHistoryVersion] = useState(0);
  const [item, setItem] = useState<Counterparty>();
  const [failure, setFailure] = useState('');
  const [items, setItems] = useState<Counterparty[]>([]);
  const [registryFilters, setRegistryFilters] = useRegistryPreference(
    preferenceKey(session.subject, 'counterparties', 'query'),
    initialCounterpartyQuery,
    restoreCounterpartyQuery,
  );
  const { sort, descending } = registryFilters;
  const filters = useColumnFilters({
    registry: 'counterparties',
    session,
    query: registryFilters,
    onChange: (filters) => setRegistryFilters((old) => ({ ...old, filters })),
    onSort: (sort, descending) =>
      setRegistryFilters((old) => ({ ...old, sort: sort as CounterpartySort, descending })),
  });
  const [nextCursor, setNextCursor] = useState<CounterpartyRegistryCursor>();
  const [more, setMore] = useState(false);
  const [loading, setLoading] = useState(true);
  const [archiving, setArchiving] = useState(false);
  const [archiveFailure, setArchiveFailure] = useState('');
  const [busy, setBusy] = useState(false);
  const epoch = useRef(0);
  useEffect(() => {
    if (item) onTitle?.(item.name);
  }, [item?.name]);
  const write = session.permissions.includes('counterparties.write');
  const read = session.permissions.includes('counterparties.read');
  const load = useCallback(
    async (cursor?: CounterpartyRegistryCursor, signal?: AbortSignal) => {
      const current = ++epoch.current;
      if (!cursor) {
        setItems([]);
        setMore(false);
      }
      setLoading(true);
      setFailure('');
      try {
        const result = await request<{
          items: Counterparty[];
          next_cursor?: CounterpartyRegistryCursor;
        }>(
          '/api/v1/counterparties/list',
          session,
          {
            filters: filterRequest(registryFilters.filters),
            sort,
            descending,
            limit: 50,
            ...(cursor ? { cursor } : {}),
          },
          signal,
        );
        if (signal?.aborted || current !== epoch.current) return;
        setItems((old) => (cursor ? [...old, ...result.items] : result.items));
        setNextCursor(result.next_cursor);
        setMore(Boolean(result.next_cursor));
      } catch (error) {
        if (!signal?.aborted && current === epoch.current) {
          setFailure(
            error instanceof Error ? error.message : translate('counterparty.couldNotLoadData'),
          );
          onError(error);
        }
      } finally {
        if (current === epoch.current) setLoading(false);
      }
    },
    [sort, descending, registryFilters.filters, session, onError],
  );
  useEffect(() => {
    if (id || creating || !read) return;
    const controller = new AbortController();
    setLoading(true);
    void load(undefined, controller.signal);
    return () => {
      controller.abort();
      epoch.current++;
    };
  }, [id, creating, read, load]);
  const refresh = useCallback(
    async (signal?: AbortSignal) => {
      if (!id) return;
      const result = await request<{ counterparty: Counterparty }>(
        '/api/v1/counterparties/get',
        session,
        { id },
        signal,
      );
      if (!signal?.aborted) {
        setItem(result.counterparty);
        setHistoryVersion((old) => old + 1);
        setFailure('');
      }
    },
    [id, session],
  );
  useEffect(() => {
    setItem(undefined);
    setTab('details');
    setFailure('');
    if (!id || !read) return;
    const controller = new AbortController();
    void refresh(controller.signal).catch((error) => {
      if (!controller.signal.aborted) {
        setFailure(error.message);
        onError(error);
      }
    });
    return () => controller.abort();
  }, [id, read, refresh, onError]);
  useEffect(() => {
    if (active)
      document.title = `${creating ? translate('entity.newCounterparty') : item?.name || translate('nav.counterparties')} · Broker`;
  }, [creating, item?.name, locale, catalogVersion]);
  async function archive() {
    if (!item) return;
    setBusy(true);
    setArchiveFailure('');
    try {
      const result = await request<{ counterparty: Counterparty }>(
        '/api/v1/counterparties/set-archived',
        session,
        { id: item.id, expected_revision: item.revision, archived: !item.archived },
      );
      setItem(result.counterparty);
      onChanged(result.counterparty);
      setArchiving(false);
    } catch (error) {
      setArchiveFailure(
        error instanceof Error ? error.message : translate('counterparty.couldNotChangeStatus'),
      );
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  if (!read)
    return (
      <section className="creation-page surface">
        <h1>{translate('nav.counterparties')}</h1>
        <p>{translate('counterparty.youDoNotHavePermissionToViewThisDirectory')}</p>
      </section>
    );
  if (creating || id)
    return (
      <section className="surface counterparty-page record-workbench">
        {failure && (
          <p role="alert" className="error">
            {failure}
          </p>
        )}
        {id && item?.id !== id ? (
          <p>
            {failure
              ? translate('counterparty.recordUnavailable')
              : translate('counterparty.openingCounterparty')}
          </p>
        ) : editing ? (
          !write || item?.archived ? (
            <p>{translate('counterparty.restoreBeforeEditing')}</p>
          ) : (
            <CounterpartyForm
              key={creating ? route : item?.id}
              initial={creating ? undefined : item}
              creationId={creationId}
              ownCompany={related?.kind === 'counterparty' && related.defaults.ownCompany}
              creationHint={<RelatedCreationHint route={route} />}
              relatedCreation={!!related}
              session={session}
              onCancel={onCancel}
              onError={onError}
              onSaved={(value) => {
                setItem(value);
                onChanged(value);
                onSaved(value);
              }}
            />
          )
        ) : (
          item && (
            <>
              <RecordHeading
                guide="counterparty"
                kind={
                  item.own_company
                    ? translate('counterparty.ownLEGALENTITY')
                    : translate('counterparty.counterpartyLEGALENTITY')
                }
                title={item.name}
                status={translate(item.archived ? 'filter.archived' : 'filter.active')}
                metadata={translate('forms.revisionValue', { revision: item.revision })}
                actions={
                  <>
                    {write && !item.archived && (
                      <a className="primary" href={`#/counterparties/${item.id}/edit`}>
                        {translate('shipment.edit')}
                      </a>
                    )}
                    {write && (
                      <button
                        onClick={() => {
                          setArchiveFailure('');
                          setArchiving(true);
                        }}
                      >
                        {item.archived
                          ? translate('counterparty.restore')
                          : translate('counterparty.archive')}
                      </button>
                    )}
                  </>
                }
              />
              {item.archived && (
                <p className="route-notice">{translate('counterparty.archivedHint')}</p>
              )}
              <CardNavigation>
                <button aria-pressed={tab === 'details'} onClick={() => switchTab('details')}>
                  {translate('counterparty.details')}
                </button>
                {session.permissions.includes('counterparties.read_bank_accounts') && (
                  <button aria-pressed={tab === 'accounts'} onClick={() => switchTab('accounts')}>
                    {translate('counterparty.bankAccounts')}
                  </button>
                )}
                {session.permissions.includes('finance.read_invoices') &&
                  session.permissions.includes('finance.read_payments') && (
                    <button
                      aria-pressed={tab === 'settlements'}
                      onClick={() => switchTab('settlements')}
                    >
                      {translate('forms.settlements')}
                    </button>
                  )}
                <button aria-pressed={tab === 'history'} onClick={() => switchTab('history')}>
                  {translate('finance.history')}
                </button>
              </CardNavigation>
              {confirmation}
              {tab === 'settlements' ? (
                <CounterpartySettlements
                  key={item.id}
                  counterpartyId={item.id}
                  counterpartyName={item.name}
                  session={session}
                  version={historyVersion}
                />
              ) : tab === 'accounts' ? (
                <BankAccounts
                  counterpartyId={item.id}
                  session={session}
                  archivedOwner={item.archived}
                  version={historyVersion}
                  onError={onError}
                  onDirty={setAccountsDirty}
                />
              ) : tab === 'history' ? (
                <CounterpartyHistory
                  key={item.id}
                  id={item.id}
                  session={session}
                  onError={onError}
                  version={historyVersion + item.revision}
                />
              ) : (
                <>
                  {item.roles.length > 0 && (
                    <div className="counterparty-roles">
                      {item.roles.map((value) => (
                        <span className="badge" key={value}>
                          {registryValueLabel('roles', value, locale) ?? counterpartyRoles[value]}
                        </span>
                      ))}
                    </div>
                  )}
                  <dl className="counterparty-details">
                    {[
                      [translate('counterparty.legalName'), item.legal_name],
                      [translate('registryField.country_code'), item.country_code],
                      [translate('counterparty.taxIDINN'), item.tax_id],
                      [translate('counterparty.kpp'), item.kpp],
                      [translate('counterparty.registrationNumberOGRN'), item.registration_id],
                      [translate('counterparty.registeredAddress'), item.legal_address],
                    ].map(([label, value]) => (
                      <div key={label}>
                        <dt>{label}</dt>
                        <dd>{value || translate('shipment.notSpecified')}</dd>
                      </div>
                    ))}
                    <div>
                      <dt>{translate('shipment.logist')}</dt>
                      <dd>
                        <EmployeeName subject={item.logist} session={session} />
                      </dd>
                    </div>
                  </dl>
                  <section className="counterparty-contacts">
                    <h2>{translate('counterparty.contacts')}</h2>
                    {!item.contacts.length ? (
                      <p className="hint">{translate('counterparty.noContactsYet')}</p>
                    ) : (
                      <div className="table-scroll">
                        <table>
                          <thead>
                            <tr>
                              <th>{translate('counterparty.name')}</th>
                              <th>{translate('counterparty.position')}</th>
                              <th>Email</th>
                              <th>{translate('counterparty.phone')}</th>
                            </tr>
                          </thead>
                          <tbody>
                            {item.contacts.map((contact, index) => (
                              <tr key={index}>
                                <td>{contact.name}</td>
                                <td>{contact.position || '—'}</td>
                                <td>{contact.email || '—'}</td>
                                <td>{contact.phone || '—'}</td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    )}
                  </section>
                </>
              )}
              {archiving && (
                <Dialog
                  title={
                    item.archived
                      ? translate('counterparty.restoreCounterparty')
                      : translate('counterparty.archiveCounterparty')
                  }
                  onClose={() => {
                    if (!busy) setArchiving(false);
                  }}
                >
                  <form
                    data-busy={busy}
                    onSubmit={(event) => {
                      event.preventDefault();
                      void archive();
                    }}
                  >
                    <p>
                      {item.archived
                        ? translate('counterparty.restoreHint')
                        : translate('counterparty.archiveHint')}
                    </p>
                    {archiveFailure && (
                      <p className="error" role="alert">
                        {archiveFailure}
                      </p>
                    )}
                    <div className="confirmation-actions">
                      <button type="button" disabled={busy} onClick={() => setArchiving(false)}>
                        {translate('counterparty.goBack')}
                      </button>
                      <button className="primary" disabled={busy}>
                        {busy
                          ? translate('shipment.saving')
                          : item.archived
                            ? translate('counterparty.restore')
                            : translate('counterparty.archive')}
                      </button>
                    </div>
                  </form>
                </Dialog>
              )}
            </>
          )
        )}
      </section>
    );
  return (
    <div className="counterparty-registry registry-page registry-workbench">
      <RegistryHeading
        columns={columnState}
        title={translate('nav.counterparties')}
        count={items.length}
        loading={loading}
        onRefresh={() => void load()}
      >
        {write && (
          <a className="button primary" href="#/counterparties/new">
            {translate('registryPage.newCounterparty')}
          </a>
        )}
      </RegistryHeading>
      <section className="surface counterparty-list registry-workbench-surface" aria-busy={loading}>
        {failure && items.length > 0 && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <RegistryTable
          items={items}
          columns={columnState}
          definitions={columns}
          filters={filters}
          sort={sort}
          descending={descending}
          onSort={(field) => {
            setRegistryFilters((old) => ({
              ...old,
              sort: field as CounterpartySort,
              descending: old.sort === field ? !old.descending : false,
            }));
          }}
          onOpen={(value) => {
            location.hash = `/counterparties/${value.id}`;
          }}
        />
        {loading && (
          <p className="directory-loading" role="status">
            {translate('counterparty.loadingCounterparties')}
          </p>
        )}
        {!items.length && !loading && (
          <RegistryResultState
            loading={loading}
            failed={Boolean(failure)}
            failureMessage={failure}
            filtered={Boolean(registryFilters.filters?.length)}
            title={translate('registryPage.noCounterparties')}
            onRetry={() => void load()}
            onReset={() => setRegistryFilters((old) => ({ ...old, filters: [] }))}
            create={
              write
                ? {
                    label: translate('registryPage.newCounterparty'),
                    onClick: () => {
                      location.hash = '/counterparties/new';
                    },
                  }
                : undefined
            }
          />
        )}
        <AutoLoadMore
          enabled={more}
          loading={loading}
          failed={Boolean(failure)}
          onLoad={() => load(nextCursor)}
        />
      </section>
    </div>
  );
}
