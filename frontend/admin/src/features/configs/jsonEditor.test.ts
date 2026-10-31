import { describe, expect, it } from 'vitest';
import { EditorState, type Transaction } from '@codemirror/state';
import { history, indentLess, indentMore, redo, undo } from '@codemirror/commands';
import { foldable, indentUnit } from '@codemirror/language';
import { json } from '@codemirror/lang-json';

describe('JSON-редактор CodeMirror', () => {
  it('находит сворачиваемые вложенные объекты', () => {
    const state = EditorState.create({
      doc: '{\n  "default": {\n    "days": 30\n  }\n}',
      extensions: [json()],
    });
    const line = state.doc.line(2);
    const range = foldable(state, line.from, line.to);
    expect(range).not.toBeNull();
    expect(state.doc.sliceString(range!.from, range!.to)).toContain('"days": 30');
  });
  it('меняет отступы выделенных строк и сохраняет историю отмены', () => {
    const doc = '{\n  "days": 30\n}';
    let state = EditorState.create({
      doc,
      selection: { anchor: 0, head: doc.length },
      extensions: [json(), history(), indentUnit.of('  ')],
    });
    const dispatch = (transaction: Transaction) => {
      state = transaction.state;
    };
    indentMore({ state, dispatch });
    expect(state.doc.toString()).toBe('  {\n    "days": 30\n  }');
    undo({ state, dispatch });
    expect(state.doc.toString()).toBe(doc);
    redo({ state, dispatch });
    expect(state.doc.toString()).toBe('  {\n    "days": 30\n  }');
    indentLess({ state, dispatch });
    expect(state.doc.toString()).toBe(doc);
  });
});
