import { interfaceText } from '../localization/interfaceText';
import { useEffect, useRef, useState, type FormEvent } from 'react';

type Field = HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement;
const invalidFields = 'input:invalid, select:invalid, textarea:invalid';

export function useFieldValidation(reveal?: (field: Field) => void) {
  const summaryForm = useRef<HTMLFormElement | null>(null);
  const [issues, setIssues] = useState<{ field: Field; label: string; message: string }[]>([]);
  function focusIssue(field: Field) {
    reveal?.(field);
    if (frame.current !== undefined) cancelAnimationFrame(frame.current);
    frame.current = requestAnimationFrame(() => {
      field.scrollIntoView({ block: 'center' });
      field.focus({ preventScroll: true });
    });
  }
  function refreshIssues() {
    if (!summaryForm.current) return;
    const next = Array.from(summaryForm.current.querySelectorAll<Field>(invalidFields)).map(
      (field) => ({
        field,
        label:
          field.getAttribute('aria-label') || field.labels?.[0]?.textContent?.trim() || field.name,
        message: field.validity.valueMissing
          ? interfaceText('validation.required')
          : field.validationMessage,
      }),
    );
    setIssues((old) =>
      old.length === next.length &&
      old.every(
        (issue, i) =>
          issue.field === next[i].field &&
          issue.label === next[i].label &&
          issue.message === next[i].message,
      )
        ? old
        : next,
    );
  }
  useEffect(refreshIssues);
  const frame = useRef<number | undefined>(undefined);
  const [attempted, setAttempted] = useState(false);
  const [errors, setErrors] = useState<Record<string, string>>({});
  useEffect(
    () => () => {
      if (frame.current !== undefined) cancelAnimationFrame(frame.current);
    },
    [],
  );
  function update(input: HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement) {
    const message = input.validity.valid
      ? ''
      : input.validity.valueMissing
        ? interfaceText('validation.required')
        : input.validationMessage;
    if (input.name) setErrors((old) => ({ ...old, [input.name]: message }));
  }
  function onInvalid(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setAttempted(true);
    const input = event.target as HTMLInputElement;
    update(input);
    const form = event.currentTarget;
    summaryForm.current = form;
    const first = form.querySelector<Field>(invalidFields);
    if (first === event.target) focusIssue(first);
  }
  function onInput(event: FormEvent<HTMLFormElement>) {
    if (!attempted) return;
    update(event.target as Field);
    refreshIssues();
  }
  function reset() {
    if (frame.current !== undefined) cancelAnimationFrame(frame.current);
    summaryForm.current = null;
    setIssues([]);
    setAttempted(false);
    setErrors({});
  }
  return {
    issues,
    focusIssue,
    attempted,
    errors,
    onInvalid,
    onInput,
    reset,
  };
}
