#include "ResultMerger.hpp"

#include "ResultRanking.hpp"

#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>

namespace altrun {
namespace {

[[nodiscard]] bool SameProviderId(
    const LauncherResult& left,
    const LauncherResult& right) {
    return !left.id.empty() &&
        !right.id.empty() &&
        left.providerId ==
            right.providerId &&
        left.id == right.id;
}

[[nodiscard]] bool DynamicDuplicatesStatic(
    const LauncherResult& dynamicResult,
    std::span<const LauncherResult>
        staticResults) {
    return std::any_of(
        staticResults.begin(),
        staticResults.end(),
        [&](const LauncherResult&
                staticResult) {
            return SameProviderId(
                       staticResult,
                       dynamicResult) ||
                SameLauncherTarget(
                    staticResult,
                    dynamicResult);
        });
}

[[nodiscard]] bool LowerTitleLess(
    std::wstring_view left,
    std::wstring_view right) {
    const auto size =
        std::min(
            left.size(),
            right.size());

    for (std::size_t i = 0;
         i < size;
         ++i) {
        const auto l =
            static_cast<wchar_t>(
                std::towlower(
                    left[i]));
        const auto r =
            static_cast<wchar_t>(
                std::towlower(
                    right[i]));

        if (l != r) {
            return l < r;
        }
    }

    return left.size() <
        right.size();
}

} // namespace

std::vector<LauncherResult>
MergeLauncherResultsRanked(
    std::span<const LauncherResult>
        staticResults,
    std::span<const LauncherResult>
        dynamicResults,
    std::size_t limit) {
    std::vector<LauncherResult> merged;
    merged.reserve(
        staticResults.size() +
        dynamicResults.size());

    for (const auto& result :
         staticResults) {
        const auto duplicate =
            std::any_of(
                merged.begin(),
                merged.end(),
                [&](const LauncherResult&
                        existing) {
                    return SameProviderId(
                               existing,
                               result) ||
                        SameLauncherTarget(
                            existing,
                            result);
                });

        if (!duplicate) {
            merged.push_back(result);
        }
    }

    for (const auto& result :
         dynamicResults) {
        if (DynamicDuplicatesStatic(
                result,
                staticResults)) {
            continue;
        }

        const auto duplicate =
            std::any_of(
                merged.begin(),
                merged.end(),
                [&](const LauncherResult&
                        existing) {
                    return SameProviderId(
                               existing,
                               result) ||
                        SameLauncherTarget(
                            existing,
                            result);
                });

        if (!duplicate) {
            merged.push_back(result);
        }
    }

    std::stable_sort(
        merged.begin(),
        merged.end(),
        [](const LauncherResult& left,
           const LauncherResult& right) {
            if (BetterLauncherResult(
                    left,
                    right)) {
                return true;
            }

            if (BetterLauncherResult(
                    right,
                    left)) {
                return false;
            }

            return LowerTitleLess(
                left.title,
                right.title);
        });

    if (merged.size() > limit) {
        merged.resize(limit);
    }

    return merged;
}

} // namespace altrun
