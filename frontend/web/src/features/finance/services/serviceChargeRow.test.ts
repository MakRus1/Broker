import { afterEach, describe, expect, it, vi } from 'vitest';
import { serviceChargeRow } from './serviceChargeRow';
import type { ServiceCharge } from './serviceCharges';
import type { MouseEvent, KeyboardEvent } from 'react';
const charge = { id: 'service', label_name: 'Фрахт' } as ServiceCharge;
afterEach(() => vi.unstubAllGlobals());
describe('service row navigation', () => {
  it('opens the same service by click and Enter', () => {
    vi.stubGlobal('window', { getSelection: () => null });
    const open = vi.fn();
    const props = serviceChargeRow(charge, open);
    props.onClick!({
      target: { closest: () => null },
    } as unknown as MouseEvent<HTMLTableRowElement>);
    const target = {};
    const preventDefault = vi.fn();
    props.onKeyDown!({
      target,
      currentTarget: target,
      key: 'Enter',
      preventDefault,
    } as unknown as KeyboardEvent<HTMLTableRowElement>);
    expect(open.mock.calls).toEqual([[charge], [charge]]);
    expect(preventDefault).toHaveBeenCalledOnce();
    expect(props.tabIndex).toBe(0);
  });
  it('keeps nested links and text selection independent', () => {
    vi.stubGlobal('window', { getSelection: () => ({ toString: () => '' }) });
    const open = vi.fn();
    const props = serviceChargeRow(charge, open);
    props.onClick!({
      target: { closest: () => ({}) },
    } as unknown as MouseEvent<HTMLTableRowElement>);
    vi.stubGlobal('window', { getSelection: () => ({ toString: () => 'selected text' }) });
    props.onClick!({
      target: { closest: () => null },
    } as unknown as MouseEvent<HTMLTableRowElement>);
    expect(open).not.toHaveBeenCalled();
  });
  it('opens a paid service for viewing without advertising editing', () => {
    vi.stubGlobal('window', { getSelection: () => null });
    const open = vi.fn();
    const props = serviceChargeRow(charge, open, true);
    expect(props['aria-label']).toBe('Просмотреть услугу «Фрахт»');
    props.onClick!({
      target: { closest: () => null },
    } as unknown as MouseEvent<HTMLTableRowElement>);
    expect(open).toHaveBeenCalledWith(charge);
  });
  it('does not advertise editing without edit access', () => {
    expect(serviceChargeRow(charge, undefined)).toEqual({});
  });
});
