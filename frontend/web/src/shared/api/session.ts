export interface Session {
  actor: string;
  subject: string;
  display_name: string;
  csrf_token: string;
  permissions: string[];
}
