export function operationStorageKey(subject: string, shipment: string) {
  return `broker.operations:${subject}:${shipment}`;
}
export function readOperationIds(key: string): string[] {
  try {
    const value: unknown = JSON.parse(localStorage.getItem(key) ?? '[]');
    return Array.isArray(value)
      ? value
          .filter((id): id is string => typeof id === 'string' && /^[\da-f-]{36}$/i.test(id))
          .slice(-100)
      : [];
  } catch {
    return [];
  }
}
