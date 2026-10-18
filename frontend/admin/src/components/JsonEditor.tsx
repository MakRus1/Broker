import { useLayoutEffect, useRef } from 'react';
import { Compartment, EditorState, Prec } from '@codemirror/state';
import { EditorView, keymap, lineNumbers } from '@codemirror/view';
import { defaultKeymap, history, historyKeymap, indentWithTab } from '@codemirror/commands';
import { json } from '@codemirror/lang-json';
import {
  defaultHighlightStyle,
  foldAll,
  foldGutter,
  foldKeymap,
  indentUnit,
  syntaxHighlighting,
  unfoldAll,
} from '@codemirror/language';

interface Props {
  value: string;
  onChange: (value: string) => void;
  disabled: boolean;
  invalid: boolean;
}

function attributes(disabled: boolean, invalid: boolean) {
  return [
    EditorState.readOnly.of(disabled),
    EditorView.editable.of(!disabled),
    EditorView.contentAttributes.of({
      id: 'value',
      'aria-label': 'Значение · JSON',
      'aria-describedby': 'value-help keyboard-help',
      'aria-invalid': String(invalid),
    }),
  ];
}

export function JsonEditor({ value, onChange, disabled, invalid }: Props) {
  const host = useRef<HTMLDivElement>(null);
  const view = useRef<EditorView | null>(null);
  const settings = useRef(new Compartment());
  const latest = useRef({ value, onChange, disabled, invalid });
  latest.current = { value, onChange, disabled, invalid };
  useLayoutEffect(() => {
    if (!host.current) return;
    const initial = latest.current;
    const editor = new EditorView({
      parent: host.current,
      state: EditorState.create({
        doc: initial.value,
        extensions: [
          json(),
          history(),
          lineNumbers(),
          foldGutter(),
          indentUnit.of('  '),
          syntaxHighlighting(defaultHighlightStyle),
          EditorView.lineWrapping,
          EditorView.cspNonce.of(
            document.querySelector<HTMLMetaElement>('meta[name="csp-nonce"]')?.content ?? '',
          ),
          EditorState.phrases.of({
            'Fold line': 'Свернуть блок',
            'Unfold line': 'Развернуть блок',
            'folded code': 'Свернутый код',
            unfold: 'Развернуть',
          }),
          keymap.of([indentWithTab, ...defaultKeymap, ...historyKeymap, ...foldKeymap]),
          Prec.highest(
            EditorView.domEventHandlers({
              keydown(event, editor) {
                if (event.key !== 'Escape') return false;
                event.preventDefault();
                event.stopPropagation();
                editor.setTabFocusMode(5000);
                return true;
              },
            }),
          ),
          settings.current.of(attributes(initial.disabled, initial.invalid)),
          EditorView.updateListener.of((update) => {
            if (update.docChanged) latest.current.onChange(update.state.doc.toString());
          }),
        ],
      }),
    });
    view.current = editor;
    return () => {
      editor.destroy();
      view.current = null;
    };
  }, []);
  useLayoutEffect(() => {
    const editor = view.current;
    if (editor && editor.state.doc.toString() !== value) {
      editor.dispatch({ changes: { from: 0, to: editor.state.doc.length, insert: value } });
    }
  }, [value]);
  useLayoutEffect(() => {
    view.current?.dispatch({
      effects: settings.current.reconfigure(attributes(disabled, invalid)),
    });
  }, [disabled, invalid]);
  return (
    <>
      <div className="editor-toolbar">
        <button
          type="button"
          disabled={disabled}
          onClick={() => {
            if (view.current) foldAll(view.current);
          }}
        >
          Свернуть всё
        </button>
        <button
          type="button"
          disabled={disabled}
          onClick={() => {
            if (view.current) unfoldAll(view.current);
          }}
        >
          Развернуть всё
        </button>
      </div>
      <div className="json-editor" ref={host} />
      <p className="muted" id="keyboard-help">
        Tab — отступ, Shift+Tab — убрать отступ. Escape, затем Tab — перейти к следующему полю.
        Стрелки слева сворачивают блоки JSON.
      </p>
    </>
  );
}
