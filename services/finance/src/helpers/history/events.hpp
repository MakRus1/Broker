#pragma once
#include <helpers/history/changes.hpp>

namespace finance::helpers::history {
using Source = models::FinancialHistorySource;

inline bool Archived(const Json& doc) {
    return At(doc, "archived").As<bool>(false);
}

inline void Fields(std::vector<models::FinancialHistoryChange>& changes, const auto& fields,
    const Json& before, const Json& after, const std::string& prefix = "",
    const std::string& label = "") {
    for (const auto& field : fields)
        AddChange(changes, field, before, after, prefix, label);
}

inline void InvoiceLines(
    std::vector<models::FinancialHistoryChange>& changes, const Json& before, const Json& after) {
    const auto old = Indexed(At(before, "charges"), "id");
    const auto current = Indexed(At(after, "charges"), "id");
    std::set<std::string> ids;
    for (const auto& [id, row] : old)
        ids.insert(id);
    for (const auto& [id, row] : current)
        ids.insert(id);
    for (const auto& id : ids) {
        const auto previous = Find(old, id);
        const auto value = Find(current, id);
        const auto label = "Услуга «" +
            (current.contains(id) ? Name(value, "label_name") : Name(previous, "label_name")) +
            "»: ";
        if (!old.contains(id) || !current.contains(id)) {
            const models::FinancialHistoryValue included{
                .format = Format::Text, .value = "Включена в счёт"};
            changes.push_back(
                {.after = current.contains(id) ? std::optional{included} : std::nullopt,
                    .before = old.contains(id) ? std::optional{included} : std::nullopt,
                    .field = "charges." + id,
                    .label = label + "состав счёта"});
        } else {
            Fields(changes, kChargeFields, previous, value, "charges." + id + ".", label);
        }
    }
}

inline std::int64_t Allocated(const Json& snapshot, const std::string& invoice) {
    if (Archived(At(snapshot, "payment")))
        return 0;
    const auto allocations = Indexed(At(snapshot, "allocations"), "invoice_id");
    return At(Find(allocations, invoice), "amount_minor").As<std::int64_t>(0);
}

inline std::optional<models::FinancialHistoryValue> Money(std::int64_t value, const Json& doc) {
    if (value == 0)
        return std::nullopt;
    return models::FinancialHistoryValue{.currency = Scalar(At(doc, "payment.currency")),
        .format = Format::Money,
        .value = std::to_string(value)};
}

inline models::FinancialHistoryEvent Event(models::FinancialHistoryCursor cursor,
    const std::string& actor, const Json& before, const Json& after, Source requested,
    const std::string& requested_id, const std::map<std::string, std::string>& invoices) {
    models::FinancialHistoryEvent event{.actor = actor,
        .cursor = cursor,
        .has_previous = !before.IsNull() && !before.IsMissing(),
        .title = ""};
    event.actor_name = Scalar(At(after, "actor_name"));
    if (cursor.source == Source::Invoice && requested == Source::ServiceCharge) {
        const auto old = Indexed(At(before, "charges"), "id");
        const auto current = Indexed(At(after, "charges"), "id");
        const bool was = old.contains(requested_id) && !Archived(At(before, "invoice"));
        const bool included = current.contains(requested_id) && !Archived(At(after, "invoice"));
        const auto old_number = Name(At(before, "invoice"), "number");
        const auto number = Name(At(after, "invoice"), "number");
        event.title = included ? "Услуга включена в счёт № " + number
                               : "Услуга исключена из счёта № " + number;
        event.changes.push_back(
            {.after = included ? std::optional{models::FinancialHistoryValue{
                                     .format = Format::Text, .value = "№ " + number}}
                               : std::nullopt,
                .before = was ? std::optional{models::FinancialHistoryValue{
                                    .format = Format::Text, .value = "№ " + old_number}}
                              : std::nullopt,
                .field = "invoice",
                .label = "Счёт"});
        return event;
    }
    if (cursor.source == Source::Payment && requested == Source::Invoice) {
        const auto old = Allocated(before, requested_id);
        const auto current = Allocated(after, requested_id);
        const auto payment = At(after, "payment");
        event.title = (current == 0 ? "Отменено распределение оплаты: "
                              : old == 0 ? "Распределена оплата: "
                                         : "Изменено распределение оплаты: ") +
            Name(payment, "number");
        event.changes.push_back({.after = Money(current, after),
            .before = Money(old, before),
            .field = "allocation",
            .label = "Оплата этого счёта"});
        return event;
    }
    const auto document = cursor.source == Source::Invoice ? At(after, "invoice")
        : cursor.source == Source::Payment                 ? At(after, "payment")
                                                           : after;
    const auto previous = cursor.source == Source::Invoice ? At(before, "invoice")
        : cursor.source == Source::Payment                 ? At(before, "payment")
                                                           : before;
    const std::string kind = cursor.source == Source::Invoice ? "Счёт"
        : cursor.source == Source::Payment
        ? (Name(document, "kind") == "receipt" ? "Поступление" : "Списание")
        : "Услуга";
    event.title = kind +
        (cursor.revision == 1                               ? ": создание"
                : Archived(document) && !Archived(previous) ? ": архивирование"
                : !Archived(document) && Archived(previous) ? ": восстановление"
                                                            : ": изменение");
    if (cursor.source == Source::ServiceCharge)
        Fields(event.changes, kChargeFields, previous, document);
    if (cursor.source == Source::Invoice) {
        Fields(event.changes, kInvoiceFields, previous, document);
        InvoiceLines(event.changes, before, after);
    }
    if (cursor.source == Source::Payment) {
        Fields(event.changes, kPaymentFields, previous, document);
        const auto old = Indexed(At(before, "allocations"), "invoice_id");
        const auto current = Indexed(At(after, "allocations"), "invoice_id");
        std::set<std::string> ids;
        for (const auto& [id, row] : old)
            ids.insert(id);
        for (const auto& [id, row] : current)
            ids.insert(id);
        for (const auto& id : ids) {
            const auto was = Allocated(before, id);
            const auto amount = Allocated(after, id);
            if (was == amount)
                continue;
            const auto name = invoices.find(id);
            event.changes.push_back({.after = Money(amount, after),
                .before = Money(was, before),
                .field = "allocations." + id,
                .label = "Оплата счёта № " + (name == invoices.end() ? id : name->second)});
        }
    }
    return event;
}
}  // namespace finance::helpers::history
