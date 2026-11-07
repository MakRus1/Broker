import { RecordGuideNotice } from './RecordGuideNotice';
import { RecordGuideMenu } from './RecordGuideMenu';
import { useLayoutEffect, useRef, useState } from 'react';
import { GuidedWorkflow } from '../GuidedWorkflow';
import { usePreferencesSession } from '../../../features/preferences/UserPreferences';
import { preferenceKey } from '../../../features/registry/registryPreferences';
import { useLocalization } from '../../../features/localization/Localization';
import {
  recordGuides,
  recordGuideDescription,
  recordTabGuides,
  sharedRecordGuideSteps,
  type RecordGuideKind,
  type RecordGuideStep,
} from './recordGuides';

export function RecordGuide({ kind }: { kind: RecordGuideKind }) {
  const { translate, locale } = useLocalization();
  const [session] = usePreferencesSession();
  const host = useRef<HTMLSpanElement>(null);
  const [context, setContext] = useState('');
  const [tabs, setTabs] = useState<string[]>([]);
  const [editing, setEditing] = useState(false);
  const [active, setActive] = useState(false);
  const [available, setAvailable] = useState<RecordGuideStep[]>([]);
  function target(step: RecordGuideStep): HTMLElement | null {
    const root = host.current?.closest('.record-workbench, .counterparty-page, form');
    if (!root) return null;
    if (step.selector)
      return (
        Array.from(root.querySelectorAll<HTMLElement>(step.selector)).find(
          (node) => node.getClientRects().length > 0,
        ) ?? null
      );
    const label = translate(step.label);
    const element = Array.from(
      root.querySelectorAll<HTMLElement>('h2,h3,dt,label,.field-label,p.hint'),
    ).find(
      (node) =>
        node.getClientRects().length > 0 &&
        node.textContent?.trim().replace(/\s*·.*$/, '') === label.replace(/\s*·.*$/, ''),
    );
    return element?.tagName === 'DT'
      ? element.parentElement
      : element?.classList.contains('field-label')
        ? (element.closest<HTMLElement>('.app-select') ?? element)
        : (element ?? null);
  }
  useLayoutEffect(() => {
    const root = host.current?.closest('.record-workbench, .counterparty-page, form');
    if (!root) return;
    const update = () => {
      setActive(root.getClientRects().length > 0);
      const isEditing =
        root.matches('form') ||
        Array.from(root.querySelectorAll<HTMLElement>('.record-layout-save')).some(
          (node) => node.getClientRects().length > 0,
        );
      setEditing(isEditing);
      const tabLabels = Array.from(
        root.querySelectorAll<HTMLElement>('.card-navigation > .tabs > button'),
      )
        .filter((node) => node.getClientRects().length > 0)
        .map((node) => node.textContent?.trim() ?? '');
      setTabs((old) => (old.join() === tabLabels.join() ? old : tabLabels));
      const selected = root.querySelector<HTMLElement>(
        '[role="tab"][aria-selected="true"], .tabs .active, .record-tabs .active, button[aria-pressed="true"]',
      );
      const navigation = Array.from(
        root.querySelectorAll<HTMLElement>('.card-navigation > .tabs > button'),
      );
      const tabIndex = selected ? navigation.indexOf(selected) : -1;
      setContext(`${isEditing ? 'edit' : 'view'}-${tabIndex < 0 ? 'main' : `tab-${tabIndex}`}`);
      const next = [...recordGuides[kind], ...sharedRecordGuideSteps]
        .map((step) => ({ step, element: target(step) }))
        .filter(
          (entry): entry is { step: RecordGuideStep; element: HTMLElement } =>
            entry.element !== null,
        )
        .sort((left, right) => {
          if (left.element === right.element) return 0;
          return left.element.compareDocumentPosition(right.element) &
            Node.DOCUMENT_POSITION_FOLLOWING
            ? -1
            : 1;
        })
        .map((entry) => entry.step);
      setAvailable((old) =>
        old.map((step) => step.id).join() === next.map((step) => step.id).join() ? old : next,
      );
    };
    update();
    const observer = new MutationObserver(update);
    observer.observe(root.closest('.workspace-content > div') ?? root, {
      subtree: true,
      childList: true,
      attributes: true,
      attributeFilter: ['hidden', 'aria-pressed'],
    });
    return () => observer.disconnect();
  }, [kind, locale]);
  return (
    <span ref={host} className="record-guide-control">
      {session && (
        <RecordGuideMenu>
          {!editing && (
            <GuidedWorkflow
              title="guide.overview"
              textButton
              active={active}
              preference={preferenceKey(session.subject, `guide-${kind}`, 'overview')}
              steps={[
                ...(tabs.length
                  ? tabs.map((label, index) => ({
                      id: `tab-${index}`,
                      title: 'guide.overview' as const,
                      titleText: label,
                      description:
                        Object.entries(recordTabGuides).find(
                          ([key]) => translate(key as keyof typeof recordTabGuides) === label,
                        )?.[1] ?? 'guide.tabMain',
                    }))
                  : available.map((step) => ({
                      id: `section-${step.id}`,
                      title: step.label,
                      description: recordGuideDescription(step, editing),
                    }))),
                {
                  id: 'overview',
                  title: 'guide.overview',
                  description: 'guide.overviewDescription',
                },
              ]}
              getTarget={(id) => {
                const root = host.current?.closest<HTMLElement>(
                  '.record-workbench, .counterparty-page, form',
                );
                if (id.startsWith('section-')) {
                  const step = available.find((step) => step.id === id.slice(8));
                  return step ? target(step) : null;
                }
                return id.startsWith('tab-')
                  ? (root?.querySelectorAll<HTMLElement>('.card-navigation > .tabs > button')[
                      Number(id.slice(4))
                    ] ?? null)
                  : host.current;
              }}
              onOpen={(_id, proceed) => proceed()}
            />
          )}
          <GuidedWorkflow
            key={context}
            title={editing ? 'guide.filling' : 'guide.section'}
            textButton
            active={active}
            preference={preferenceKey(session.subject, `guide-${kind}-${context}`, 'workflow')}
            steps={available.map((step) => ({
              id: step.id,
              title: step.label,
              description: recordGuideDescription(step, editing),
            }))}
            getTarget={(id) => {
              const step = available.find((step) => step.id === id);
              return step ? target(step) : null;
            }}
            onOpen={(_id, proceed) => proceed()}
          />
        </RecordGuideMenu>
      )}
      <RecordGuideNotice context={context} active={active} editing={editing} />
    </span>
  );
}
