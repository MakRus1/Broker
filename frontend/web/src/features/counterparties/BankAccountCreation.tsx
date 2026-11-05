import { RelatedCreationHint, useRelatedRequests } from '../../shared/forms/RelatedCreation';
import { useLocalization } from '../localization/Localization';
import { RecordHeading } from '../../shared/ui/record/RecordLayout';
import { useEffect, useState } from 'react';
import { request as apiRequest } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { BankAccounts, type BankAccount } from './BankAccounts';

const ignore = () => {};
export function BankAccountCreation({
  route,
  session,
  onSaved,
  onCancel,
  onError,
}: {
  route: string;
  session: Session;
  onSaved: (value: BankAccount) => void;
  onCancel: () => void;
  onError: (error: unknown) => void;
}) {
  const { translate } = useLocalization();
  const request = useRelatedRequests()?.pending.get(route);
  const owner = request?.kind === 'bankAccount' ? request.defaults.owner : '';
  const [ownerName, setOwnerName] = useState('');
  useEffect(() => {
    if (!owner) return;
    const controller = new AbortController();
    void apiRequest<{ counterparty: { name: string } }>(
      '/api/v1/counterparties/get',
      session,
      { id: owner },
      controller.signal,
    )
      .then(({ counterparty }) => {
        if (!controller.signal.aborted) setOwnerName(counterparty.name);
      })
      .catch(() => {});
    return () => controller.abort();
  }, [owner, session]);
  return (
    <section className="surface record-workbench">
      <RecordHeading
        kind={translate('counterparty.bankAccounts')}
        title={translate('counterparty.newBankAccount')}
        metadata={ownerName ? translate('related.accountOwner', { name: ownerName }) : undefined}
      />
      <RelatedCreationHint route={route} />
      {request?.kind === 'bankAccount' ? (
        <BankAccounts
          counterpartyId={request.defaults.owner}
          session={session}
          archivedOwner={false}
          version={0}
          onError={onError}
          onDirty={ignore}
          creation={{
            id: route.split('/')[3],
            currency: request.defaults.currency,
            onSaved,
            onCancel,
          }}
        />
      ) : (
        <>
          <p role="alert">{translate('related.contextLost')}</p>
          <button onClick={onCancel}>{translate('action.cancel')}</button>
        </>
      )}
    </section>
  );
}
