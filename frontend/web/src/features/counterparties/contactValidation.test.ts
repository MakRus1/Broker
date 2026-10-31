import { describe, expect, it } from 'vitest';
import { isEmail, isPhone } from './counterparties';

describe('Контактные данные', () => {
  it.each(['anna@example.test', 'a+b@sub.example.test', 'name@почта.рф'])(
    'принимает email %s',
    (email) => expect(isEmail(email)).toBe(true),
  );
  it.each([
    'a@@example.test',
    'a@',
    '@example.test',
    'a@example',
    'a@.test',
    'a@example.',
    'a b@example.test',
    '.a@example.test',
    'a..b@example.test',
    'a@-example.test',
  ])('отклоняет email %s', (email) => expect(isEmail(email)).toBe(false));
  it.each(['+7 (900) 000-00-00', '88001234567', '123-45-67', '+86 138 0000 0000'])(
    'принимает телефон %s',
    (phone) => expect(isPhone(phone)).toBe(true),
  );
  it.each([
    '123',
    'call me',
    '1234567890123456',
    '8+9000000000',
    '+7 (900 000-00-00',
    '+7 ((900)) 000-00-00',
  ])('отклоняет телефон %s', (phone) => expect(isPhone(phone)).toBe(false));
});
