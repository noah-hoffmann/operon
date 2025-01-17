// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright 2019-2021 Heal Research

#ifndef OPERON_PARETO_NONDOMINATED_SORTER_BASE
#define OPERON_PARETO_NONDOMINATED_SORTER_BASE

#include "core/individual.hpp"

namespace Operon {

namespace detail {
    template <typename T, std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>, bool> = true>
    static inline size_t count_trailing_zeros(T block)
    {
        assert(block != T{0}); // output is undefined for 0!

        constexpr size_t u_bits_number = std::numeric_limits<unsigned>::digits;
        constexpr size_t ul_bits_number = std::numeric_limits<unsigned long>::digits;
        constexpr size_t ull_bits_number = std::numeric_limits<unsigned long long>::digits;
        const size_t bits_per_block = sizeof(T) * CHAR_BIT;

#if defined(__clang__) || defined(__GNUC__)
        if constexpr (bits_per_block <= u_bits_number) {
            return static_cast<size_t>(__builtin_ctz(static_cast<unsigned int>(block)));
        } else if constexpr (bits_per_block <= ul_bits_number) {
            return static_cast<size_t>(__builtin_ctzl(static_cast<unsigned long>(block)));
        } else if constexpr (bits_per_block <= ull_bits_number) {
            return static_cast<size_t>(__builtin_ctzll(static_cast<unsigned long long>(block)));
        }
#elif defined(_MSC_VER)
#if defined(_M_IX86) || defined(_M_ARM) || defined(_M_X64) || defined(_M_ARM64)
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#endif
#if (defined(_M_X64) || defined(_M_ARM64))
#pragma intrinsic(_BitScanForward64)
#endif
        constexpr size_t ul_bits_number = std::numeric_limits<unsigned long>::digits;
        constexpr size_t ui64_bits_number = std::numeric_limits<unsigned __int64>::digits;
        if constexpr (bits_per_block <= ul_bits_number) {
            unsigned long index = std::numeric_limits<unsigned long>::max();
            _BitScanForward(&index, static_cast<unsigned long>(block));
            return static_cast<size_type>(index);
        } else if constexpr (bits_per_block <= ui64_bits_number) {
#if DYNAMIC_BITSET_CAN_USE_MSVC_BUILTIN_BITSCANFORWARD64
            unsigned long index = std::numeric_limits<unsigned long>::max();
            _BitScanForward64(&index, static_cast<unsigned __int64>(block));
            return static_cast<size_type>(index);
#else
            constexpr unsigned long max_ul = std::numeric_limits<unsigned long>::max();
            unsigned long low = block & max_ul;
            if (low != 0) {
                unsigned long index = std::numeric_limits<unsigned long>::max();
                _BitScanForward(&index, low);
                return static_cast<size_type>(index);
            }
            unsigned long high = block >> ul_bits_number;
            unsigned long index = std::numeric_limits<unsigned long>::max();
            _BitScanForward(&index, high);
            return static_cast<size_type>(ul_bits_number + index);
#endif
        }
#endif
        T mask = T { 1 };
        for (size_t i = 0; i < bits_per_block; ++i) {
            if ((block & mask) != 0) {
                return i;
            }
            mask <<= 1;
        }
    }
} // namespace detail

class NondominatedSorterBase {
    public:
        using Result = std::vector<std::vector<size_t>>;

        mutable struct {
            size_t LexicographicalComparisons = 0; // both lexicographical and single-objective
            size_t SingleValueComparisons = 0;
            size_t DominanceComparisons = 0;
            size_t RankComparisons = 0;
            size_t InnerOps = 0;
            double MeanRank  = 0;
            double MeanND = 0;
        } Stats;

        void Reset() { Stats = {0, 0, 0, 0, 0, 0., 0.}; }

        virtual Result Sort(Operon::Span<Operon::Individual const>) const = 0;

        // if indices are provided, they are assumed to be sorted
        // if the span is empty, then indices are generated and sorted on the fly
        Result operator()(Operon::Span<Operon::Individual const> pop) const {
            size_t m = pop.front().Fitness.size();
            ENSURE(m > 1);
            return Sort(pop);
        }
};

} // namespace Operon

#endif
