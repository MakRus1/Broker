import { useLocalization } from '../../features/localization/Localization';

export function ClearSelectionButton({
  label,
  actionLabel,
  disabled,
  onClick,
}: {
  label: string;
  actionLabel?: string;
  disabled?: boolean;
  onClick: () => void;
}) {
  const { translate } = useLocalization();
  return (
    <button
      type="button"
      className="lookup-field-clear"
      disabled={disabled}
      aria-label={actionLabel ?? translate('lookup.clearField', { label })}
      title={actionLabel ?? translate('lookup.clearField', { label })}
      onMouseDown={(event) => event.preventDefault()}
      onClick={onClick}
    >
      <svg
        width="16"
        height="16"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        aria-hidden="true"
      >
        <path d="m6 6 12 12M18 6 6 18" />
      </svg>
    </button>
  );
}
