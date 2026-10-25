#pragma once

#include <models/json.hpp>
#include <optional>
#include <string>

namespace configs {
enum class DraftStatus { Draft, Published, Cancelled };

struct Draft {
    std::int64_t Id;
    std::string ConfigName;
    std::string Author;
    std::string Description;
    std::int64_t Version;
    DraftStatus Status;
    std::optional<std::string> ApprovedBy;
    std::int64_t BaseVersion;
    std::optional<Json> BaseValue;
    std::optional<Json> Value;
    std::optional<std::int64_t> SourceRevision;
    std::optional<std::int64_t> PublishedRevision;
    std::optional<std::string> AuthorName;
    std::optional<std::string> ApprovedByName;
};

struct CreateDraftRequest {
    std::string Actor;
    std::string ConfigName;
    std::string Description;
    std::int64_t ExpectedConfigVersion;
    std::optional<Json> Value;
    std::optional<std::int64_t> SourceRevision;
    std::optional<std::string> ActorName = std::nullopt;
    bool Remove = false;
};

struct EditDraftRequest {
    std::string Actor;
    std::int64_t Id;
    std::int64_t ExpectedVersion;
    std::optional<Json> Value;
    std::optional<std::string> Description;
};

struct DraftActionRequest {
    std::string Actor;
    std::int64_t Id;
    std::int64_t ExpectedVersion;
    std::optional<std::string> ActorName = std::nullopt;
};

struct DraftFilter {
    std::optional<std::string> ConfigName;
    std::optional<std::int64_t> BeforeId;
};
}  // namespace configs
