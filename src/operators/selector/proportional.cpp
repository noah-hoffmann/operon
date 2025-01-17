// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright 2019-2021 Heal Research

#include "operators/selection.hpp"

namespace Operon {

size_t
ProportionalSelector::operator()(Operon::RandomGenerator& random) const
{
    std::uniform_real_distribution<Operon::Scalar> uniformReal(0, fitness.back().first - std::numeric_limits<Operon::Scalar>::epsilon());
    return std::lower_bound(fitness.begin(), fitness.end(), std::make_pair(uniformReal(random), size_t{0}), std::less {})->second;
}

void ProportionalSelector::Prepare(const Operon::Span<const Individual> pop) const
{
    SelectorBase::Prepare(pop);
    Prepare();
}

void ProportionalSelector::Prepare() const
{
    fitness.clear();
    fitness.reserve(this->population.size());

    Operon::Scalar vmin = this->population[0][idx], vmax = vmin;
    for (size_t i = 0; i < this->population.size(); ++i) {
        auto f = this->population[i][idx];
        fitness.push_back({ f, i });
        vmin = std::min(vmin, f);
        vmax = std::max(vmax, f);
    }
    auto prepare = [=](auto p) { return std::make_pair(vmax - p.first, p.second); };
    std::transform(fitness.begin(), fitness.end(), fitness.begin(), prepare);
    std::sort(fitness.begin(), fitness.end());
    std::inclusive_scan(fitness.begin(), fitness.end(), fitness.begin(), [](auto lhs, auto rhs) { return std::make_pair(lhs.first + rhs.first, rhs.second); });
}
}

