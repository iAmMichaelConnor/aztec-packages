#include "barretenberg/vm2/constraining/flavor.hpp"
#include "barretenberg/polynomials/fold_stride2.hpp"

#include <gtest/gtest.h>
#include <memory>

namespace bb::avm2 {

class AvmFirstFoldTests : public ::testing::TestWithParam<bool> {};

TEST_P(AvmFirstFoldTests, MatchesReferenceForSparseAndEmptySources)
{
    using Polynomial = AvmFlavor::Polynomial;
    using FF = AvmFlavor::FF;
    constexpr size_t circuit_size = 64;
    auto full = std::make_unique<AvmFlavor::ProverPolynomials>();
    full->memory_optimizations = GetParam();
    auto sources = full->get_all();
    const std::array<std::pair<size_t, size_t>, 8> ranges{
        { { 0, 0 }, { 7, 0 }, { 0, 1 }, { 1, 1 }, { 0, 9 }, { 1, 8 }, { 2, 7 }, { 7, 9 } }
    };
    for (size_t j = 0; j < ranges.size(); ++j) {
        const auto [start, size] = ranges[j];
        sources[j] = Polynomial(size, circuit_size, start);
        for (size_t i = start; i < start + size; ++i) {
            sources[j].at(i) = FF(i + 3);
        }
    }

    for (const FF challenge : { FF::zero(), FF::one(), FF(17) }) {
        auto folded = std::make_unique<AvmFlavor::ProverPolynomials>(*full, circuit_size);
        auto destinations = folded->get_all();
        for (size_t j = 0; j < ranges.size(); ++j) {
            SCOPED_TRACE(j);
            const auto& source = sources[j];
            auto& destination = destinations[j];
            const size_t end = (source.end_index() + 1) / 2;
            ASSERT_EQ(destination.size(), end);
            if (!GetParam() || source.is_empty() || source.start_index() >= 2) {
                for (size_t i = 0; i < end; ++i) {
                    EXPECT_EQ(destination[i], FF::zero());
                }
            }
            fold_stride2(source, destination, 0, end, challenge);
            for (size_t i = 0; i < end; ++i) {
                const FF expected = source[2 * i] + challenge * (source[2 * i + 1] - source[2 * i]);
                EXPECT_EQ(destination[i], expected);
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(MemoryOptimizations, AvmFirstFoldTests, ::testing::Bool());

} // namespace bb::avm2
