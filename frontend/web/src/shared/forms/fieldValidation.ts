import { interfaceText } from '../localization/interfaceText';
import { useEffect, useRef, useState, type FormEvent } from 'react';

export function useFieldValidation() {
  const frame = useRef<number | undefined>(undefined);
  const [invalid, setInvalid] = useState(false);
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
    setInvalid(true);
    const input = event.target as HTMLInputElement;
    update(input);
    const form = event.currentTarget;
    if (frame.current !== undefined) cancelAnimationFrame(frame.current);
    frame.current = requestAnimationFrame(() => {
      const first = form.querySelector<HTMLElement>(':invalid:not(fieldset)');
      first?.scrollIntoView({ block: 'center', behavior: 'auto' });
      first?.focus({ preventScroll: true });
    });
  }
  function onInput(event: FormEvent<HTMLFormElement>) {
    if (attempted) {
      const form = event.currentTarget;
      update(event.target as HTMLInputElement);
      if (frame.current !== undefined) cancelAnimationFrame(frame.current);
      frame.current = requestAnimationFrame(() =>
        setInvalid(!!form.querySelector(':invalid:not(fieldset)')),
      );
    }
  }
  function reset() {
    if (frame.current !== undefined) cancelAnimationFrame(frame.current);
    setInvalid(false);
    setAttempted(false);
    setErrors({});
  }
  return {
    attempted,
    errors,
    message: invalid ? interfaceText('validation.checkFields') : '',
    onInvalid,
    onInput,
    reset,
  };
}
