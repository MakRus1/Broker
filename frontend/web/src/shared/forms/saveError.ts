import { ApiError } from '../api/http';
import { interfaceText } from '../localization/interfaceText';

export function saveErrorMessage(error: unknown, fallback: string): string {
  if (error instanceof ApiError && error.status === 409 && error.code === 'revision_conflict')
    return interfaceText('validation.revisionConflict');
  return error instanceof Error ? error.message : fallback;
}
