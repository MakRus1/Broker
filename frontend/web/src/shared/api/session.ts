export interface Session {
  actor: string;
  subject: string;
  organization_id: string;
  display_name: string;
  csrf_token: string;
  permissions: string[];
}
