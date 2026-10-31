import { useState, type FormEvent } from 'react';

export function useFieldValidation() {
  const [attempted, setAttempted] = useState(false);
  const [errors, setErrors] = useState<Record<string, string>>({});
  function update(input: HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement) {
    const message = input.validity.valid
      ? ''
      : input.validity.valueMissing
        ? 'Заполните обязательное поле'
        : input.validationMessage;
    if (input.name) setErrors((old) => ({ ...old, [input.name]: message }));
  }
  function onInvalid(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setAttempted(true);
    const input = event.target as HTMLInputElement;
    update(input);
    const form = event.currentTarget;
    requestAnimationFrame(() => {
      const first = form.querySelector<HTMLElement>(':invalid:not(fieldset)');
      first?.scrollIntoView({ block: 'center', behavior: 'smooth' });
      first?.focus({ preventScroll: true });
    });
  }
  function onInput(event: FormEvent<HTMLFormElement>) {
    if (attempted) update(event.target as HTMLInputElement);
  }
  function reset() {
    setAttempted(false);
    setErrors({});
  }
  return { attempted, errors, onInvalid, onInput, reset };
}
