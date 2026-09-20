#include "core/ResultIconPipeline.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    const auto small =
        MakeResultIconCacheKey(
            L"C:\\Tools\\app.exe",
            14);
    const auto large =
        MakeResultIconCacheKey(
            L"C:\\Tools\\app.exe",
            20);

    assert(small != large);
    assert(
        small ==
        MakeResultIconCacheKey(
            L"C:\\Tools\\app.exe",
            14));

    const ResultIconRequestStamp
        current{42, 7, 20};

    assert(
        ShouldAcceptResultIconCompletion(
            true,
            current,
            42,
            7));

    assert(
        !ShouldAcceptResultIconCompletion(
            false,
            current,
            42,
            7));

    assert(
        !ShouldAcceptResultIconCompletion(
            true,
            current,
            43,
            7));

    assert(
        !ShouldAcceptResultIconCompletion(
            true,
            current,
            42,
            8));

    const ResultIconRequestStamp
        invalid{42, 7, 0};

    assert(
        !ShouldAcceptResultIconCompletion(
            true,
            invalid,
            42,
            7));

    static_assert(
        kResultIconCacheCapacity >= 64);
    static_assert(
        kResultIconCacheCapacity <= 128);

    std::cout
        << "Result icon pipeline tests passed\n";
    return 0;
}
