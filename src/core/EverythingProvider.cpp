#include "EverythingProvider.hpp"

#include "ProviderIds.hpp"
#include "ResultRanking.hpp"

#include <algorithm>
#include <chrono>
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

} // namespace

EverythingProvider::EverythingProvider(
    EverythingIpcClientOptions options)
    : client_(options), probeClient_([&] {
          options.debounce = std::chrono::milliseconds(0);
          options.sendTimeout = std::chrono::milliseconds(100);
          options.replyTimeout = std::chrono::milliseconds(200);
          return options;
      }()) {}

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

void EverythingProvider::ProbeContinuation(
    std::uint64_t generation, std::wstring query, ProbeCompletion completion) {
    auto search = classic_behavior::BuildFilenameContinuationQuery(query);
    if (search.empty()) {
        completion(generation, classic_behavior::ContinuationEvidence::Unknown);
        return;
    }
    probeClient_.QueryAsync({generation, std::move(search), 16},
        [query = std::move(query), completion = std::move(completion)](EverythingQueryResult result) {
            using classic_behavior::ContinuationEvidence;
            auto evidence = ContinuationEvidence::Unknown;
            if (result.status == EverythingQueryStatus::Success) {
                for (const auto& item : result.items) {
                    if (classic_behavior::StrongNameContinuation(item.name, query)) {
                        evidence = ContinuationEvidence::Present;
                        break;
                    }
                }
                // A bounded, filtered page with no match is not proof of absence.
                if (evidence != ContinuationEvidence::Present && result.totalMatches == 0 && result.items.empty())
                    evidence = ContinuationEvidence::Absent;
            }
            completion(result.generation, evidence);
        });
}

void EverythingProvider::QueryAsync(DynamicQueryRequest request, Completion completion) {
    const auto prefixQuery = classic_behavior::BuildFilenameContinuationQuery(request.query);
    if (prefixQuery.empty()) {
        QueryBroad(std::move(request), std::move(completion));
        return;
    }
    EverythingQueryRequest prefix{request.generation, prefixQuery,
        static_cast<std::uint32_t>(std::clamp<std::size_t>(request.limit, 16, 1000))};
    client_.QueryAsync(std::move(prefix),
        [this, request = std::move(request), completion = std::move(completion)](EverythingQueryResult found) mutable {
            std::vector<LauncherResult> prefixes;
            if (found.status == EverythingQueryStatus::Success) {
                for (auto& item : found.items) {
                    LauncherResult result;
                    result.id = EverythingResultId(item.fullPath);
                    result.providerId = std::string(providers::kEverythingFilesystem);
                    result.kind = item.kind == EverythingItemKind::Folder ? ResultKind::Folder : ResultKind::File;
                    result.title = std::move(item.name);
                    result.subtitle = std::move(item.parentPath);
                    result.target = std::move(item.fullPath);
                    result.detail = result.target;
                    result.action.kind = result.kind == ResultKind::Folder ? LauncherActionKind::OpenFolder : LauncherActionKind::OpenFile;
                    result.action.payload = result.target;
                    if (RankDynamicResultText(result, request.query)) prefixes.push_back(std::move(result));
                }
            }
            QueryBroad(std::move(request), std::move(completion), std::move(prefixes));
        });
}

void EverythingProvider::QueryBroad(
    DynamicQueryRequest request, Completion completion,
    std::vector<LauncherResult> prefixes) {
    EverythingQueryRequest ipcRequest;
    ipcRequest.generation =
        request.generation;
    std::wstring rankingQuery =
        request.query;

    const std::size_t outputLimit =
        request.limit;

    // Everything truncates before ALTRun Next applies its own relevance
    // policy. For broad short queries that can starve the post-filtered set:
    // a relevant prefix such as v2rayN may sit outside Everything's first
    // few dozen rows while unrelated path/name hits occupy the initial page.
    // Overfetch a bounded candidate pool, rank locally, then trim back to the
    // launcher's requested candidate count.
    const std::size_t scaledLimit =
        outputLimit >= 125
            ? 1000
            : outputLimit * 8;
    const std::size_t fetchLimit =
        std::min<std::size_t>(
            1000,
            std::max<std::size_t>(
                200,
                scaledLimit));

    ipcRequest.query =
        std::move(request.query);
    ipcRequest.limit =
        static_cast<std::uint32_t>(
            fetchLimit);

    client_.QueryAsync(
        std::move(ipcRequest),
        [completion =
             std::move(completion),
         rankingQuery =
             std::move(rankingQuery),
         outputLimit,
         prefixes = std::move(prefixes)](
            EverythingQueryResult
                ipcResult) mutable {
            DynamicQueryResponse response;
            response.generation =
                ipcResult.generation;
            response.providerId =
                std::string(
                    providers::
                        kEverythingFilesystem);
            response.status =
                MapStatus(
                    ipcResult.status);
            response.totalMatches =
                ipcResult.totalMatches;
            response.latencyMicros =
                static_cast<std::uint64_t>(
                    std::max<
                        std::int64_t>(
                        0,
                        ipcResult.latency
                            .count()));
            response.nativeError =
                ipcResult.nativeError;

            response.results = std::move(prefixes);
            response.results.reserve(
                ipcResult.items.size());

            for (auto& item :
                 ipcResult.items) {
                LauncherResult result;
                result.id =
                    EverythingResultId(
                        item.fullPath);
                result.providerId =
                    std::string(
                        providers::
                            kEverythingFilesystem);
                result.kind =
                    item.kind ==
                            EverythingItemKind::
                                Folder
                        ? ResultKind::Folder
                        : ResultKind::File;
                result.title =
                    std::move(item.name);
                result.subtitle =
                    std::move(
                        item.parentPath);
                result.target =
                    std::move(
                        item.fullPath);
                result.detail =
                    result.target;
                if (!RankDynamicResultText(
                        result,
                        rankingQuery)) {
                    continue;
                }

                result.action.kind =
                    result.kind ==
                            ResultKind::Folder
                        ? LauncherActionKind::
                            OpenFolder
                        : LauncherActionKind::
                            OpenFile;
                result.action.payload =
                    result.target;

                if (std::none_of(response.results.begin(), response.results.end(),
                        [&](const LauncherResult& existing) { return existing.id == result.id; }))
                    response.results.push_back(std::move(result));
            }

            std::stable_sort(
                response.results.begin(),
                response.results.end(),
                [](const LauncherResult& left,
                   const LauncherResult& right) {
                    return BetterLauncherResult(
                        left,
                        right);
                });

            if (response.results.size() >
                outputLimit) {
                response.results.resize(
                    outputLimit);
            }

            if (completion) {
                completion(
                    std::move(response));
            }
        });
}

} // namespace altrun
