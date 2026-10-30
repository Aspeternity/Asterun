#include "EverythingProvider.hpp"

#include "EverythingIpcProtocol.hpp"
#include "ProviderIds.hpp"
#include "RelevancePolicy.hpp"
#include "ResultRanking.hpp"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace altrun {
namespace {

[[nodiscard]]
DynamicQueryStatus MapStatus(
    EverythingQueryStatus status) {
    switch (status) {
    case EverythingQueryStatus::Success:
        return DynamicQueryStatus::Success;
    case EverythingQueryStatus::Unavailable:
        return DynamicQueryStatus::Unavailable;
    case EverythingQueryStatus::SendTimeout:
    case EverythingQueryStatus::ReplyTimeout:
        return DynamicQueryStatus::Timeout;
    case EverythingQueryStatus::ProtocolError:
    case EverythingQueryStatus::Cancelled:
        return DynamicQueryStatus::Error;
    }

    return DynamicQueryStatus::Error;
}

[[nodiscard]] std::wstring
EverythingResultId(
    std::wstring_view fullPath) {
    std::wstring id =
        L"everything.filesystem:";
    id.append(fullPath);
    return id;
}

[[nodiscard]] LauncherResult MakeFileResult(
    const EverythingIpcItem& item) {
    LauncherResult result;
    result.id = EverythingResultId(item.fullPath);
    result.providerId = std::string(providers::kEverythingFilesystem);
    result.kind = item.kind == EverythingItemKind::Folder
        ? ResultKind::Folder
        : ResultKind::File;
    result.title = item.name;
    result.subtitle = item.parentPath;
    result.target = item.fullPath;
    result.detail = result.target;
    result.action.kind = result.kind == ResultKind::Folder
        ? LauncherActionKind::OpenFolder
        : LauncherActionKind::OpenFile;
    result.action.payload = result.target;
    return result;
}

} // namespace

EverythingProvider::EverythingProvider(
    EverythingIpcClientOptions options)
    : client_(options), prefixClient_(options) {}

std::string_view
EverythingProvider::Id() const noexcept {
    return providers::
        kEverythingFilesystem;
}

bool EverythingProvider::IsAvailable()
    const noexcept {
    return client_.IsAvailable();
}

EverythingIpcStatusSnapshot
EverythingProvider::Status() const {
    return client_.Status();
}

void EverythingProvider::QueryAsync(DynamicQueryRequest request, Completion completion) {
    // Two Everything queries per keystroke: a name-prefix pass (so a strong
    // "v2rayN" is never starved by a broad page) and the broad query. They
    // run on separate IPC clients at the same time; the reply is assembled
    // once both arrive. Both ask for newest-modified first, so Everything's
    // truncated page keeps recent files instead of the alphabetical head.
    struct Join {
        std::mutex mutex;
        int remaining{0};
        std::vector<LauncherResult> prefixes;
        std::optional<EverythingQueryResult> broad;
        DynamicQueryRequest request;
        Completion completion;
    };
    auto join = std::make_shared<Join>();
    const auto prefixQuery =
        classic_behavior::BuildFilenameContinuationQuery(request.query);
    join->remaining = prefixQuery.empty() ? 1 : 2;

    // Everything truncates before Asterun applies its own relevance policy.
    // Overfetch a bounded candidate pool, rank locally, then trim back to
    // the launcher's requested candidate count.
    const std::size_t scaledLimit =
        request.limit >= 125 ? 1000 : request.limit * 8;
    EverythingQueryRequest broad;
    broad.generation = request.generation;
    broad.query = request.query;
    broad.limit = static_cast<std::uint32_t>(std::min<std::size_t>(
        1000, std::max<std::size_t>(200, scaledLimit)));
    broad.sortType = everything_ipc::kSortDateModifiedDescending;

    EverythingQueryRequest prefix;
    prefix.generation = request.generation;
    prefix.query = prefixQuery;
    prefix.limit = static_cast<std::uint32_t>(
        std::clamp<std::size_t>(request.limit, 16, 1000));
    prefix.sortType = everything_ipc::kSortDateModifiedDescending;

    join->request = std::move(request);
    join->completion = std::move(completion);

    const auto finish = [](const std::shared_ptr<Join>& state) {
        // Called with state->mutex released, exactly once per query.
        DynamicQueryResponse response;
        const EverythingQueryResult& ipc = *state->broad;
        response.generation = ipc.generation;
        response.providerId = std::string(providers::kEverythingFilesystem);
        response.status = MapStatus(ipc.status);
        response.totalMatches = ipc.totalMatches;
        response.latencyMicros = static_cast<std::uint64_t>(
            std::max<std::int64_t>(0, ipc.latency.count()));
        response.nativeError = ipc.nativeError;
        response.results = std::move(state->prefixes);

        std::unordered_set<std::wstring> seenIds;
        seenIds.reserve(response.results.size() + ipc.items.size());
        for (const auto& result : response.results) seenIds.insert(result.id);
        for (const auto& item : ipc.items) {
            LauncherResult result = MakeFileResult(item);
            if (!RankDynamicResultText(result, state->request.query)) continue;
            if (seenIds.insert(result.id).second)
                response.results.push_back(std::move(result));
        }

        std::stable_sort(response.results.begin(), response.results.end(),
            [](const LauncherResult& left, const LauncherResult& right) {
                return BetterLauncherResult(left, right);
            });
        // Many copies of one file name (build outputs, caches, backups)
        // crowd out everything else. Keep the best-ranked one of each name;
        // both passes are newest first, so ties keep the newest copy.
        // Explicit Everything syntax asks for every match.
        if (!relevance::HasExplicitSyntax(state->request.query)) {
            std::unordered_set<std::wstring> seenNames;
            std::erase_if(response.results, [&](const LauncherResult& result) {
                std::wstring key = result.kind == ResultKind::Folder ? L"d|" : L"f|";
                for (const wchar_t ch : result.title) {
                    key.push_back(static_cast<wchar_t>(std::towlower(ch)));
                }
                return !seenNames.insert(std::move(key)).second;
            });
        }
        if (response.results.size() > state->request.limit) {
            response.results.resize(state->request.limit);
        }
        if (state->completion) state->completion(std::move(response));
    };

    if (!prefixQuery.empty()) {
        prefixClient_.QueryAsync(std::move(prefix),
            [join, finish](EverythingQueryResult found) {
                std::vector<LauncherResult> prefixes;
                if (found.status == EverythingQueryStatus::Success) {
                    for (const auto& item : found.items) {
                        LauncherResult result = MakeFileResult(item);
                        if (RankDynamicResultText(result, join->request.query))
                            prefixes.push_back(std::move(result));
                    }
                }
                bool done = false;
                {
                    std::scoped_lock lock(join->mutex);
                    join->prefixes = std::move(prefixes);
                    done = --join->remaining == 0;
                }
                if (done) finish(join);
            });
    }

    client_.QueryAsync(std::move(broad),
        [join, finish](EverythingQueryResult found) {
            bool done = false;
            {
                std::scoped_lock lock(join->mutex);
                join->broad = std::move(found);
                done = --join->remaining == 0;
            }
            if (done) finish(join);
        });
}

} // namespace altrun
