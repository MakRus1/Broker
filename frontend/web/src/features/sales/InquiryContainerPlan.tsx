import { useContainerTypes } from '../../shared/api/useContainerTypes';
import { useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { RecordSection } from '../../shared/ui/record/RecordLayout';
import { Select } from '../../shared/ui/Select';
import { useLocalization } from '../localization/Localization';
import type { ShipmentSettings } from '../shipments/ShipmentWorkspace';
import type { PlannedContainer } from './types';

const ownerships = ['COC', 'SOC'] as const;

export function InquiryContainerPlan({
  active,
  session,
  customerId,
  items = [],
  onChange,
}: {
  active: boolean;
  session: Session;
  customerId: string;
  items?: PlannedContainer[];
  onChange?: (items: PlannedContainer[]) => void;
}) {
  const { translate } = useLocalization();
  const [loaded, setLoaded] = useState<{ customerId: string; settings: ShipmentSettings }>();
  const [failed, setFailed] = useState<string>();
  const canRead = session.permissions.includes('shipments.read');
  useEffect(() => {
    if (!active || !canRead || !customerId) return;
    const controller = new AbortController();
    setLoaded(undefined);
    setFailed(undefined);
    request<ShipmentSettings>(
      '/api/v1/shipments/settings',
      session,
      { customer_id: customerId },
      controller.signal,
    )
      .then((settings) => {
        if (!controller.signal.aborted) setLoaded({ customerId, settings });
      })
      .catch(() => {
        if (!controller.signal.aborted) setFailed(customerId);
      });
    return () => controller.abort();
  }, [active, canRead, customerId, session]);

  const settings = loaded?.customerId === customerId ? loaded.settings : undefined;
  const { types, failure: catalogFailure } = useContainerTypes(
    session,
    active && Boolean(onChange) && settings?.planned_containers === true,
  );

  if (!customerId || !canRead) return null;
  if (failed === customerId)
    return <p className="hint">{translate('sales.containerPlanSettingsUnavailable')}</p>;
  if (!settings?.planned_containers || (!onChange && items.length === 0)) return null;

  const choices = types.flatMap((container_type_code) =>
    ownerships.map((container_ownership) => ({ container_type_code, container_ownership })),
  );
  const next = choices.find(
    (choice) =>
      !items.some(
        (item) =>
          item.container_type_code === choice.container_type_code &&
          item.container_ownership === choice.container_ownership,
      ),
  );
  return (
    <RecordSection title={translate('shipment.plannedContainers')}>
      {catalogFailure && <p className="hint">{catalogFailure}</p>}
      {onChange ? (
        <>
          {items.map((item, index) => {
            const update = (change: Partial<PlannedContainer>) =>
              onChange(
                items.map((row, position) => (position === index ? { ...row, ...change } : row)),
              );
            return (
              <div className="record-fields record-fields-two" key={index}>
                <Select
                  label={translate('registryField.container_type')}
                  value={item.container_type_code}
                  options={[...new Set([...types, item.container_type_code])].map((value) => ({
                    value,
                    label: value,
                  }))}
                  onChange={(container_type_code) => update({ container_type_code })}
                />
                <Select
                  label={translate('registryField.container_ownership')}
                  value={item.container_ownership}
                  options={ownerships.map((value) => ({ value, label: value }))}
                  onChange={(value) =>
                    update({ container_ownership: value === 'SOC' ? 'SOC' : 'COC' })
                  }
                />
                <label>
                  {translate('shipment.plannedQuantity')}
                  <input
                    type="number"
                    min={1}
                    max={10000}
                    required
                    value={item.planned_quantity || ''}
                    onChange={(event) => update({ planned_quantity: Number(event.target.value) })}
                  />
                </label>
                <button type="button" onClick={() => onChange(items.filter((_, i) => i !== index))}>
                  {translate('shipment.remove')}
                </button>
              </div>
            );
          })}
          <button
            type="button"
            disabled={!next || items.length >= 100}
            onClick={() => {
              if (next) onChange([...items, { ...next, planned_quantity: 1 }]);
            }}
          >
            {translate('shipment.addItem')}
          </button>
        </>
      ) : (
        <ul>
          {items.map((item) => (
            <li key={item.container_type_code + item.container_ownership}>
              {item.planned_quantity} × {item.container_type_code} {item.container_ownership}
            </li>
          ))}
        </ul>
      )}
    </RecordSection>
  );
}
