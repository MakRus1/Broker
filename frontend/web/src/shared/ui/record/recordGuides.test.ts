import { expect, it } from 'vitest';
import { recordGuides, sharedRecordGuideSteps, recordGuideDescription } from './recordGuides';
import { translate } from '../../../../../shared/i18n/catalog';

it('provides unique, translated steps for every supported record form', () => {
  expect(Object.keys(recordGuides)).toHaveLength(8);
  for (const steps of [...Object.values(recordGuides), sharedRecordGuideSteps]) {
    expect(new Set(steps.map((step) => step.id)).size).toBe(steps.length);
    for (const step of steps) {
      for (const locale of ['ru', 'en'] as const) {
        expect(translate(locale, step.label)).not.toBe(step.label);
        expect(translate(locale, step.description)).not.toBe(step.description);
      }
    }
  }
});

it('uses separate contract explanations in view and edit modes', () => {
  for (const step of recordGuides.contract) {
    expect(recordGuideDescription(step, false)).not.toBe(recordGuideDescription(step, true));
    for (const locale of ['ru', 'en'] as const)
      expect(translate(locale, recordGuideDescription(step, false))).not.toBe(
        recordGuideDescription(step, false),
      );
  }
});
