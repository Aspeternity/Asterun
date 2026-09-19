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
    : client_(std::move(options)) {}

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

void EverythingProvider::QueryAsync(
    DynamicQueryRequest request,
    Completion completion) {
    EverythingQueryRequest ipcRequest;
    ipcRequest.generation =
        request.generation;
    std::wstring rankingQuery =
        request.query;

    ipcRequest.query =
        std::move(request.query);
    ipcRequest.limit =
        static_cast<std::uint32_t>(
            std::min<std::size_t>(
                request.limit,
                1000));

    client_.QueryAsync(
        std::move(ipcRequest),
        [completion =
             std::move(completion),
         rankingQuery =
             std::move(rankingQuery)](
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
                result.score =
                    ScoreDynamicResultText(
                        result,
                        rankingQuery);
                result.action.kind =
                    result.kind ==
                            ResultKind::Folder
                        ? LauncherActionKind::
                            OpenFolder
                        : LauncherActionKind::
                            OpenFile;
                result.action.payload =
                    result.target;

                response.results.push_back(
                    std::move(result));
            }

            if (completion) {
                completion(
                    std::move(response));
            }
        });
}

} // namespace altrun
