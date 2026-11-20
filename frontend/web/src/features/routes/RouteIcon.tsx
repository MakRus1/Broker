const paths: Record<string, string> = {
  sea: 'M12 3v5 M8 8h8v5 M3 14l9-3 9 3-3 6H6z M2 21q2-2 4 0t4 0t4 0t4 0t4 0',
  air: 'M22 3L9 16 M22 3l-5 18-4-8-8-4z M7 17l-4 4',
  rail: 'M6 3h12v14H6z M6 10h12 M12 3v7 M8 17l-3 4 M16 17l3 4 M9 14h.01 M15 14h.01',
  road: 'M3 6h11v11H3z M14 10h4l3 4v3h-7 M6 20a2 2 0 1 0 0-4 2 2 0 0 0 0 4 M18 20a2 2 0 1 0 0-4 2 2 0 0 0 0 4',
};
export function RouteIcon({ type }: { type: string }) {
  const mode = type.includes('rail') ? 'rail' : type === 'sea' || type === 'air' ? type : 'road';
  return (
    <svg
      className="route-mode-icon"
      width="20"
      height="20"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.5"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d={paths[mode]} />
    </svg>
  );
}
