export type ActionDue = 'overdue' | 'today' | 'soon';
export function nextActionDue(date: string | undefined, now = new Date()): ActionDue | undefined {
  if (!date || !/^\d{4}-\d{2}-\d{2}$/.test(date)) return undefined;
  const today = Date.UTC(now.getFullYear(), now.getMonth(), now.getDate());
  const days = (Date.parse(`${date}T00:00:00Z`) - today) / 86400000;
  if (days < 0) return 'overdue';
  if (days === 0) return 'today';
  if (days <= 3) return 'soon';
  return undefined;
}
