import { useLocalization } from '../localization/Localization';
import type { CounterpartyContact } from './counterparties';
import './counterpartyContacts.css';

export function CounterpartyContacts({
  contacts,
  selectedId,
}: {
  contacts: CounterpartyContact[];
  selectedId?: string;
}) {
  const { translate } = useLocalization();
  if (!contacts.length) return <p className="hint">{translate('counterparty.noContactsYet')}</p>;
  return (
    <div className="table-scroll">
      <table className="counterparty-contacts-table">
        <thead>
          <tr>
            <th>{translate('counterparty.contactName')}</th>
            <th>{translate('counterparty.position')}</th>
            <th>Email</th>
            <th>{translate('counterparty.phone')}</th>
          </tr>
        </thead>
        <tbody>
          {contacts.map((contact, index) => (
            <tr
              key={contact.id ?? index}
              className={
                selectedId && contact.id === selectedId ? 'counterparty-contact-current' : undefined
              }
            >
              <td>
                {contact.name}
                {selectedId && contact.id === selectedId && (
                  <span className="counterparty-contact-badge">
                    <svg
                      width="12"
                      height="12"
                      viewBox="0 0 16 16"
                      fill="none"
                      stroke="currentColor"
                      strokeWidth="1.8"
                      aria-hidden="true"
                    >
                      <path d="m3 8 3 3 7-7" />
                    </svg>
                    {translate('sales.contactSelected')}
                  </span>
                )}
              </td>
              <td>{contact.position || '—'}</td>
              <td>
                {contact.email ? (
                  <a className="link" href={`mailto:${encodeURIComponent(contact.email)}`}>
                    {contact.email}
                  </a>
                ) : (
                  '—'
                )}
              </td>
              <td>
                {contact.phone ? (
                  <a className="link" href={`tel:${contact.phone.replace(/[^\d+]/g, '')}`}>
                    {contact.phone}
                  </a>
                ) : (
                  '—'
                )}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
