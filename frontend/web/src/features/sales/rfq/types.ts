import type { FinancialLabel } from '../../finance/labels/financialLabels';
import type { ProposalPrice } from '../proposals';
import type { ShipmentRouteSegment } from '../../routes/types';
export type RfqQuoteLine = {
  id: string;
  label_id: string;
  label_name?: string;
  unit: FinancialLabel['unit'];
  quantity_milli: number;
  price: ProposalPrice;
};
export type RfqQuote = {
  id: string;
  title: string;
  supplier_id: string;
  supplier_name?: string;
  valid_until?: string;
  delivery_days?: number;
  note?: string;
  lines: RfqQuoteLine[];
};
export type RfqInput = {
  id: string;
  inquiry_id: string;
  route_segment_id: string;
  title: string;
  responsible_subject: string;
  answer_by?: string;
  note?: string;
  status: 'in_progress' | 'ready' | 'cancelled';
  quotes: RfqQuote[];
};
export type Rfq = {
  input: RfqInput;
  revision: number;
  route_segment: ShipmentRouteSegment;
  inquiry_number: string;
};
