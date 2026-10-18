#pragma once

#include <optional>
#include <platform/configs/runtime.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace configs {
using Json = platform::configs::Json;

struct Revision {
    std::int64_t Id;
    Json Values;
};
struct ConfigValues {
    Json Values;
    std::int64_t Revision;
    std::string UpdatedAt;
};
struct CatalogEntry {
    std::string Name;
    std::int64_t Version;
    std::optional<std::int64_t> ActiveDraftId;
};
struct Config {
    std::string Name;
    Json Value;
    std::int64_t Version;
    std::optional<std::int64_t> ActiveDraftId;
};
struct RevisionInfo {
    std::int64_t Id;
    std::string CreatedAt;
};
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
    Json BaseValue;
    Json Value;
    std::optional<std::int64_t> SourceRevision;
    std::optional<std::int64_t> PublishedRevision;
};
struct CreateDraftRequest {
    std::string Actor;
    std::string ConfigName;
    std::string Description;
    std::int64_t ExpectedConfigVersion;
    std::optional<Json> Value;
    std::optional<std::int64_t> SourceRevision;
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
};
struct DraftFilter {
    std::optional<std::string> ConfigName;
    std::optional<std::int64_t> BeforeId;
};
struct DraftEvent {
    std::string Action;
    std::string Actor;
    Draft Snapshot;
    std::string CreatedAt;
};
class InvalidRequest : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class RevisionConflict : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class DraftForbidden : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class RevisionNotFound : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
}  // namespace configs
