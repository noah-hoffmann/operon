// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright 2019-2021 Heal Research

#ifndef JSF_HPP
#define JSF_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

// implementation of Bob Jenkins' small prng https://burtleburtle.net/bob/rand/smallprng.html
// the name JSF (Jenkins Small Fast) was coined by Doty-Humphrey when he included it in PractRand
// a more detailed analysis at http://www.pcg-random.org/posts/bob-jenkins-small-prng-passes-practrand.html
namespace Operon {
namespace Random {
    namespace detail {
        using _rand32_underlying = uint32_t;
        using _rand64_underlying = uint64_t;

        template <size_t N>
        struct rand_type {
            using type = void;
        };
        template <>
        struct rand_type<32> {
            using type = _rand32_underlying;
        };
        template <>
        struct rand_type<64> {
            using type = _rand64_underlying;
        };
    }

    // public
    template <size_t N>
    using rand_t = typename detail::rand_type<N>::type;
    using rand32_t = rand_t<32>;
    using rand64_t = rand_t<64>;

    // bitwise circular left shift.
    template <size_t N>
    static inline rand_t<N> rotl(rand_t<N> x, rand_t<N> k) noexcept { return (x << k) | (x >> (N - k)); }

    template <size_t N>
    class JsfRand final {
    private:
        rand_t<N> a, b, c, d;

        // 2-rotate version for 32-bit with the amounts (27, 7)
        inline rand32_t prng32() noexcept
        {
            rand32_t e = a - rotl<N>(b, 27);
            a = b ^ rotl<N>(c, 17);
            b = c + d;
            c = d + e;
            d = e + a;
            return d;
        }

        // 3-rotate version for 64-bit with the amounts (7, 13, 37) yielding 18.4 bits of avalanche after 5 rounds
        inline rand64_t prng64() noexcept
        {
            rand64_t e = a - rotl<N>(b, 7);
            a = b ^ rotl<N>(c, 13);
            b = c + rotl<N>(d, 37);
            c = d + e;
            d = e + a;
            return d;
        }

    public:
        using result_type = rand_t<N>;
        static constexpr result_type min() { return result_type { 0 }; }
        static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

        explicit JsfRand(result_type seed = 0xdeadbeef) noexcept
            : a { 0xf1ea5eed }
            , b { seed }
            , c { seed }
            , d { seed }
        {
            static_assert(N == 32 || N == 64, "Invalid template parameter. Valid values are 32 and 64 for 32-bit and 64-bit output.");
            for (size_t i = 0; i < 20; ++i) {
                (*this)();
            }
        }

        // disallow copying (to prevent misuse)
        JsfRand(JsfRand const&) = delete;
        JsfRand& operator=(JsfRand const&) = delete;

        // allow moving
        JsfRand(JsfRand&&) noexcept = default;
        JsfRand& operator=(JsfRand&&) noexcept = default;

        ~JsfRand() noexcept = default;

        inline rand_t<N> operator()() noexcept
        {
            if constexpr (N == 32)
                return prng32();
            return prng64();
        }
    };

    using Jsf32 = JsfRand<32>;
    using Jsf64 = JsfRand<64>;
}
}

#endif
