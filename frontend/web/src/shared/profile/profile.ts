export function profileName(name: string): string {
  const parts = name.trim().split(/\s+/);
  if (parts.length === 1) return parts[0];
  return `${parts[0]} ${parts
    .slice(1)
    .map((part) => `${Array.from(part)[0]}.`)
    .join(' ')}`;
}
