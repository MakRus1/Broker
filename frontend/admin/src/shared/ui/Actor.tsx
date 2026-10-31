import type { Session } from '../api/api';

export function actorName(actor: string, name?: string, session?: Session) {
  return name || (actor === session?.actor ? session.display_name : undefined) || 'Имя недоступно';
}

export function Actor({
  actor,
  name,
  session,
}: {
  actor: string;
  name?: string;
  session?: Session;
}) {
  return <span title={actor}>{actorName(actor, name, session)}</span>;
}
