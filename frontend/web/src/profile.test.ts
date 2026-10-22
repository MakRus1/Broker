import { expect, it } from 'vitest';
import { profileName } from './profile';
it('сокращает любое ФИО до фамилии и инициалов', () =>
  expect(profileName('Иванов Иван Иванович')).toBe('Иванов И. И.'));
it('сокращает длинное ФИО до фамилии и инициалов', () =>
  expect(profileName('Александров Александр Александрович')).toBe('Александров А. А.'));
it('не придумывает ФИО из логина', () => expect(profileName('author')).toBe('author'));

it('поддерживает фамилию и имя без отчества', () =>
  expect(profileName('Петрова Анна')).toBe('Петрова А.'));
