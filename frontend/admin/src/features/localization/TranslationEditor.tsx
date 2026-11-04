import { useEffect, useMemo, useState } from 'react';
import { request, type Session } from '../../shared/api/api';
import { Select } from '../../shared/ui/Select';
import type { Organization } from '../organizations/types';
import {
  builtins,
  decode,
  encode,
  parameters,
  same,
  type EditorState,
  type Locale,
  type Values,
} from './catalog';

const pageSize = 30;
export function TranslationEditor({
  session,
  organizations,
  onDirtyChange,
}: {
  session: Session;
  organizations: Organization[];
  onDirtyChange: (value: boolean) => void;
}) {
  const [scope, setScope] = useState('');
  const [state, setState] = useState<EditorState>();
  const [values, setValues] = useState<Values>({ ru: {}, en: {} });
  const [query, setQuery] = useState('');
  const [changedOnly, setChangedOnly] = useState(false);
  const [page, setPage] = useState(0);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  const writable = session.permissions.includes('localization.write');
  const dirty = state ? !same(values, decode(state.draft ?? state.published)) : false;
  const publishedChanged = state ? !same(values, decode(state.published)) : false;
  const accept = (next: EditorState) => {
    setState(next);
    setValues(decode(next.draft ?? next.published));
  };
  useEffect(() => {
    const controller = new AbortController();
    setState(undefined);
    setError('');
    setNotice('');
    setBusy(true);
    request<EditorState>(
      '/api/admin/localization/get',
      session,
      { organization_id: scope || undefined },
      controller.signal,
    )
      .then(accept)
      .catch((reason) => {
        if (!controller.signal.aborted)
          setError(reason instanceof Error ? reason.message : 'Не удалось загрузить переводы');
      })
      .finally(() => {
        if (!controller.signal.aborted) setBusy(false);
      });
    return () => controller.abort();
  }, [scope, session]);
  useEffect(() => {
    onDirtyChange(dirty);
    return () => onDirtyChange(false);
  }, [dirty, onDirtyChange]);
  useEffect(() => {
    if (!dirty) return;
    const prevent = (event: BeforeUnloadEvent) => event.preventDefault();
    window.addEventListener('beforeunload', prevent);
    return () => window.removeEventListener('beforeunload', prevent);
  }, [dirty]);
  const inherited = useMemo(() => (state ? decode(state.inherited) : { ru: {}, en: {} }), [state]);
  const keys = useMemo(
    () =>
      Object.keys(builtins.ru)
        .filter((key) => {
          if (scope && (key.startsWith('login.') || key.startsWith('keycloak.'))) return false;
          if (changedOnly && !values.ru[key] && !values.en[key]) return false;
          const text = [
            key,
            builtins.ru[key as keyof typeof builtins.ru],
            builtins.en[key as keyof typeof builtins.en],
            values.ru[key],
            values.en[key],
            inherited.ru[key],
            inherited.en[key],
          ]
            .join(' ')
            .toLowerCase();
          return text.includes(query.toLowerCase());
        })
        .sort(),
    [query, changedOnly, scope, values, inherited],
  );
  const invalid = useMemo(
    () =>
      (['ru', 'en'] as Locale[]).flatMap((locale) =>
        Object.entries(values[locale])
          .filter(
            ([key, value]) =>
              value &&
              parameters(value) !== parameters(builtins[locale][key as keyof typeof builtins.ru]),
          )
          .map(([key]) => key),
      ),
    [values],
  );
  const edit = (locale: Locale, key: string, value: string) => {
    setValues((previous) => ({ ...previous, [locale]: { ...previous[locale], [key]: value } }));
    setNotice('');
  };
  async function perform(action: 'save' | 'publish') {
    if (!state) return;
    setBusy(true);
    setError('');
    setNotice('');
    try {
      const next = await request<EditorState>(`/api/admin/localization/${action}`, session, {
        organization_id: scope || undefined,
        version: state.version,
        ...(action === 'save' ? { catalog: encode(values) } : {}),
      });
      accept(next);
      setNotice(
        action === 'save'
          ? 'Черновик сохранён. Пользователи пока видят опубликованные тексты.'
          : `Опубликована ревизия ${next.revision}. Открытые страницы получат переводы в течение минуты.`,
      );
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Не удалось сохранить переводы');
    } finally {
      setBusy(false);
    }
  }
  return (
    <section className="translation-editor">
      <div className="heading">
        <div>
          <p className="eyebrow">ЛОКАЛИЗАЦИЯ</p>
          <h1>Переводы</h1>
          <p>
            Пустое значение возвращает общий или встроенный текст. Параметры в фигурных скобках
            нужно сохранить.
          </p>
        </div>
      </div>
      <div className="translation-controls">
        <Select
          label="Каталог"
          value={scope}
          options={[
            { value: '', label: 'Общие переводы' },
            ...organizations.map((x) => ({ value: x.id, label: x.name })),
          ]}
          onChange={(next) => {
            if (!dirty || window.confirm('Перейти без сохранения изменений?')) {
              setScope(next);
              setPage(0);
            }
          }}
        />
        <label>
          Поиск по ключу или тексту
          <input
            type="search"
            value={query}
            onChange={(e) => {
              setQuery(e.target.value);
              setPage(0);
            }}
          />
        </label>
        <label className="translation-checkbox">
          <input
            type="checkbox"
            checked={changedOnly}
            onChange={(e) => {
              setChangedOnly(e.target.checked);
              setPage(0);
            }}
          />
          Только переопределённые
        </label>
      </div>
      {error && (
        <p role="alert" className="error">
          {error}
        </p>
      )}
      {notice && <p role="status">{notice}</p>}
      {!state ? (
        <p role="status">{busy ? 'Загрузка переводов…' : 'Каталог не загружен'}</p>
      ) : (
        <>
          <p className="translation-status">
            Ревизия {state.revision} · {state.draft ? 'Есть сохранённый черновик' : 'Черновика нет'}{' '}
            · Найдено ключей: {keys.length}
          </p>
          <div className="translation-rows" aria-busy={busy}>
            {keys.slice(page * pageSize, (page + 1) * pageSize).map((key) => (
              <article className="translation-row" key={key}>
                <code>{key}</code>
                <div className="translation-languages">
                  {(['ru', 'en'] as Locale[]).map((locale) => {
                    const fallback =
                      inherited[locale][key] ?? builtins[locale][key as keyof typeof builtins.ru];
                    const mismatch =
                      !!values[locale][key] &&
                      parameters(values[locale][key]) !== parameters(fallback);
                    return (
                      <label key={locale}>
                        {locale === 'ru' ? 'Русский' : 'English'}
                        <textarea
                          rows={2}
                          value={values[locale][key] ?? ''}
                          disabled={!writable || busy}
                          maxLength={10000}
                          aria-invalid={mismatch || undefined}
                          aria-label={`${key} ${locale}`}
                          onChange={(e) => edit(locale, key, e.target.value)}
                        />
                        <span className="translation-fallback">
                          {scope ? 'Наследуется' : 'Встроенный текст'}: {fallback}
                        </span>
                        {mismatch && (
                          <span role="alert">
                            Сохраните параметры: {parameters(fallback) || 'без параметров'}
                          </span>
                        )}
                        {values[locale][key] && writable && (
                          <button
                            type="button"
                            className="link"
                            disabled={busy}
                            onClick={() => edit(locale, key, '')}
                          >
                            Вернуть исходный текст
                          </button>
                        )}
                      </label>
                    );
                  })}
                </div>
              </article>
            ))}
          </div>
          <div className="translation-pagination">
            <button disabled={page === 0} onClick={() => setPage(page - 1)}>
              Назад
            </button>
            <span>
              Страница {page + 1} из {Math.max(1, Math.ceil(keys.length / pageSize))}
            </span>
            <button
              disabled={(page + 1) * pageSize >= keys.length}
              onClick={() => setPage(page + 1)}
            >
              Далее
            </button>
          </div>
          {writable && (
            <div className="translation-actions">
              <span>
                {dirty ? 'Есть несохранённые изменения' : 'Изменения сохранены'}
                {invalid.length > 0 ? ' · Проверьте параметры переводов' : ''}
              </span>
              <button
                disabled={busy || !dirty || invalid.length > 0}
                onClick={() => perform('save')}
              >
                Сохранить черновик
              </button>
              <button
                className="primary"
                disabled={busy || dirty || !state.draft || !publishedChanged || invalid.length > 0}
                onClick={() => perform('publish')}
              >
                Опубликовать
              </button>
            </div>
          )}
        </>
      )}
    </section>
  );
}
