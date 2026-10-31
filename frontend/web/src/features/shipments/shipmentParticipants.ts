type ShipmentParticipants = { customer_id: string; recipient_id: string };

export function changeShipmentCustomer(
  current: ShipmentParticipants,
  customer: string,
): ShipmentParticipants {
  // Подставленный получатель следует за клиентом; ручной выбор и явную очистку сохраняем.
  return {
    customer_id: customer,
    recipient_id: current.recipient_id === current.customer_id ? customer : current.recipient_id,
  };
}
