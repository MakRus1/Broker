import { useEffect, useLayoutEffect, useRef, useState, type InputHTMLAttributes } from 'react';
import { displayDateInput, parseDateInput, maskDateInput } from './dateText';

type Props = Omit<
  InputHTMLAttributes<HTMLInputElement>,
  'type' | 'value' | 'onChange' | 'min' | 'max'
> & {
  type?: 'date' | 'datetime-local';
  value: string;
  min?: string;
  max?: string;
  onChange: (value: string) => void;
};

export function DateInput({ type = 'date', value, onChange, min, max, ...props }: Props) {
  const [text, setText] = useState(() => displayDateInput(value));
  const [touched, setTouched] = useState(false);
  const input = useRef<HTMLInputElement>(null);
  const caret = useRef<number | undefined>(undefined);
  const picker = useRef<HTMLInputElement>(null);
  const emitted = useRef(value);
  const withTime = type === 'datetime-local';
  const parsed = parseDateInput(text, withTime);
  const failure =
    parsed === undefined
      ? `Введите дату в формате ${withTime ? 'ДД.ММ.ГГГГ ЧЧ:ММ' : 'ДД.ММ.ГГГГ'}`
      : parsed && min && parsed < min
        ? `Дата должна быть не раньше ${displayDateInput(min)}`
        : parsed && max && parsed > max
          ? `Дата должна быть не позже ${displayDateInput(max)}`
          : '';
  useEffect(() => {
    if (value !== emitted.current) {
      emitted.current = value;
      setText(displayDateInput(value));
    }
  }, [value]);
  useEffect(() => {
    input.current?.setCustomValidity(failure);
  }, [failure]);
  useLayoutEffect(() => {
    if (caret.current !== undefined) {
      input.current?.setSelectionRange(caret.current, caret.current);
      caret.current = undefined;
    }
  });
  function change(next: string) {
    setText(next);
    const result = parseDateInput(next, withTime);
    emitted.current = result ?? '';
    onChange(result ?? '');
  }
  return (
    <span className="date-input-control">
      <input
        {...props}
        ref={input}
        type="text"
        inputMode="numeric"
        autoComplete="off"
        placeholder={withTime ? 'ДД.ММ.ГГГГ ЧЧ:ММ' : 'ДД.ММ.ГГГГ'}
        value={text}
        aria-invalid={Boolean(touched && text && failure) || props['aria-invalid'] || undefined}
        onChange={(event) => {
          const field = event.currentTarget;
          const edit = maskDateInput(
            field.value,
            field.selectionStart ?? field.value.length,
            withTime,
            (event.nativeEvent as InputEvent).inputType?.startsWith('delete'),
          );
          caret.current = edit.cursor;
          change(edit.text);
        }}
        onKeyDown={(event) => {
          const field = event.currentTarget;
          const start = field.selectionStart;
          if (start === null || start !== field.selectionEnd) return;
          // Backspace/Delete удаляют цифру соседней части, а разделитель сохраняется.
          if (event.key === 'Backspace' && /[. :]/.test(field.value[start - 1] ?? ''))
            field.setSelectionRange(start - 1, start - 1);
          if (event.key === 'Delete' && /[. :]/.test(field.value[start] ?? ''))
            field.setSelectionRange(start + 1, start + 1);
          props.onKeyDown?.(event);
        }}
        onInvalid={() => setTouched(true)}
        onBlur={(event) => {
          setTouched(true);
          if (parsed !== undefined) setText(displayDateInput(parsed));
          props.onBlur?.(event);
        }}
      />
      <input
        ref={picker}
        className="date-input-native"
        type={type}
        tabIndex={-1}
        aria-hidden="true"
        disabled={props.disabled}
        value={parsed !== undefined && !failure ? parsed : ''}
        min={min}
        max={max}
        onChange={(event) => change(displayDateInput(event.target.value))}
      />
      <button
        type="button"
        className="date-input-calendar"
        aria-label="Открыть календарь"
        disabled={props.disabled}
        onClick={() => picker.current?.showPicker()}
      >
        <svg
          viewBox="0 0 24 24"
          width="18"
          height="18"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.6"
          aria-hidden="true"
        >
          <rect x="3" y="5" width="18" height="16" rx="2" />
          <path d="M7 3v4M17 3v4M3 11h18" />
        </svg>
      </button>
    </span>
  );
}
