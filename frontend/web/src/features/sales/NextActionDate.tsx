import { useEffect, useState } from 'react';
import { useLocalization } from '../localization/Localization';
import { nextActionDue } from './nextActionDue';
import './nextActionDate.css';

export function NextActionDate({ date }: { date?: string }) {
  const { translate, locale } = useLocalization();
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const timer = window.setInterval(() => setNow(new Date()), 60000);
    return () => window.clearInterval(timer);
  }, []);
  if (!date) return <span>—</span>;
  const due = nextActionDue(date, now);
  return (
    <span className={`sales-action-date${due ? ` sales-action-date--${due}` : ''}`}>
      <time dateTime={date}>
        {new Intl.DateTimeFormat(locale).format(new Date(`${date}T00:00:00`))}
      </time>
      {due && (
        <span className="sales-action-date-label">{translate(`sales.actionDue.${due}`)}</span>
      )}
    </span>
  );
}
