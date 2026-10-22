export interface Organization {
  id: string;
  name: string;
  enabled: boolean;
  revision: number;
}
export interface Member {
  subject: string;
  username: string;
  display_name: string;
  last_name?: string | null;
  first_name?: string | null;
  middle_name?: string | null;
  email: string;
  role: string;
  enabled: boolean;
  revision: number;
}
export interface OrganizationAccess {
  platform_admin: boolean;
  organizations: Organization[];
  managed_organization_ids: string[];
}
export interface OrganizationRole {
  id: string;
  name: string;
  description: string;
  permissions: string[];
  revision: number;
}
export interface Permission {
  dependencies: string[];
  id: string;
  name: string;
  group: string;
}
export interface MemberInput {
  mode: 'create' | 'existing' | 'edit';
  username: string;
  last_name: string;
  first_name: string;
  middle_name: string;
  email: string;
  temporary_password: string;
  role: string;
  enabled: boolean;
}
