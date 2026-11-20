const paths: Record<string, string> = {
  '/sales': 'M4 4h16v4H4z M6 8v12h12V8 M9 12h6 M9 16h4',
  '/inquiries': 'M4 5h16v14H4z M4 6l8 7 8-7',
  '/shipments': 'M3 7l9-4 9 4v10l-9 4-9-4z M3 7l9 4 9-4 M12 11v10',
  '/transportations':
    'M3 6h11v11H3z M14 10h4l3 4v3h-7 M6 20a2 2 0 1 0 0-4 2 2 0 0 0 0 4 M18 20a2 2 0 1 0 0-4 2 2 0 0 0 0 4',
  '/service-charges': 'M5 4h14v16H5z M8 8h8 M8 12h8 M8 16h5',
  '/outgoing-invoices': 'M5 3h9l5 5v13H5z M14 3v5h5 M9 16l6-6 M10 10h5v5',
  '/incoming-invoices': 'M5 3h9l5 5v13H5z M14 3v5h5 M15 11l-6 6 M9 12v5h5',
  '/receipts': 'M4 5h16v14H4z M12 8v8 M8 12l4 4 4-4',
  '/disbursements': 'M4 5h16v14H4z M12 16V8 M8 12l4-4 4 4',
  '/counterparties': 'M4 21V7l8-4v18 M12 10h8v11 M2 21h20 M7 8v1 M7 12v1 M7 16v1 M16 13v1 M16 17v1',
  '/contracts': 'M6 3h9l4 4v14H6z M15 3v5h4 M9 12h7 M9 16h5',
  '/financial-labels': 'M3 4h9l9 9-8 8-10-10z M7 8h.01',
};

export function NavigationIcon({ route }: { route: string }) {
  return (
    <svg
      width="18"
      height="18"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d={paths[route]} />
    </svg>
  );
}
