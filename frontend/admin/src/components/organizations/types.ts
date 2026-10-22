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
  id: string;
  name: string;
  group: string;
}
export interface MemberInput {
  mode: 'create' | 'existing' | 'edit';
  username: string;
  display_name: string;
  email: string;
  temporary_password: string;
  role: string;
  enabled: boolean;
}
