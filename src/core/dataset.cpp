// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright 2019-2021 Heal Research

#include "core/dataset.hpp"
#include <fmt/core.h>

#include "core/constants.hpp"
#include "core/types.hpp"
#include "hash/hash.hpp"

#include "vstat.hpp"
#include <csv/parser.hpp>
#include <fast_float/fast_float.h>

namespace Operon {

// internal implementation details
namespace {
    const auto defaultVariables = [](size_t count) {
        Hasher<HashFunction::XXHash> hash;

        std::vector<Variable> vars(count);
        for (size_t i = 0; i < vars.size(); ++i) {
            vars[i].Name = fmt::format("X{}", i+1);
            vars[i].Index = i;
            vars[i].Hash = hash(reinterpret_cast<uint8_t const*>(vars[i].Name.c_str()), vars[i].Name.size());
        }
        std::sort(vars.begin(), vars.end(), [](auto &a, auto &b) { return a.Hash < b.Hash; });
        return vars;
    };
}

Dataset::Matrix Dataset::ReadCsv(std::string const& path, bool hasHeader)
{
    std::ifstream f(path);
    aria::csv::CsvParser parser(f);

    auto nrow = std::count(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>(), '\n');
    // rewind the ifstream
    f.clear();
    f.seekg(0);

    auto ncol = 0ul;

    Hasher<HashFunction::XXHash> hash;
    Matrix m;

    if (hasHeader) {
        --nrow; // for matrix allocation, don't care about column names
        for (auto& row : parser) {
            for (auto& field : row) {
                auto h = hash(reinterpret_cast<uint8_t const*>(field.c_str()), field.size());
                Variable v { field, h, ncol++ };
                variables.push_back(v);
            }
            break; // read only the first row
        }
        std::sort(variables.begin(), variables.end(), [](auto& a, auto& b) { return a.Hash < b.Hash; });
        m.resize(nrow, ncol);
    }

    std::vector<Operon::Scalar> vec;
    size_t rowIdx = 0;

    for (auto& row : parser) {
        size_t fieldIdx = 0;
        for (auto& field : row) {
            Operon::Scalar v;
            auto status = fast_float::from_chars(field.data(), field.data() + field.size(), v);
            if(status.ec != std::errc()) {
                throw std::runtime_error(fmt::format("failed to parse field {} at line {}\n", fieldIdx, rowIdx));
            }
            vec.push_back(v);
            ++fieldIdx;
        }
        if (ncol == 0) {
            ENSURE(!hasHeader);
            ncol = vec.size();
            m.resize(nrow, ncol);
            variables = defaultVariables(ncol);
        }
        m.row(rowIdx) = Eigen::Map<Eigen::Array<Operon::Scalar, Eigen::Dynamic, 1>>(vec.data(), vec.size());
        vec.clear();
        ++rowIdx;
    }
    return m;
}

Dataset::Dataset(std::vector<std::vector<Operon::Scalar>> const& vals)
    : Dataset(defaultVariables(vals.size()), vals)
{
}

Dataset::Dataset(std::string const& path, bool hasHeader)
    : values(ReadCsv(path, hasHeader))
    , map(values.data(), values.rows(), values.cols())
{
}

Dataset::Dataset(Matrix&& vals)
    : variables(defaultVariables((size_t)vals.cols()))
    , values(std::move(vals))
    , map(values.data(), values.rows(), values.cols())
{
}

Dataset::Dataset(Matrix const& vals)
    : variables(defaultVariables((size_t)vals.cols()))
    , values(vals)
    , map(values.data(), values.rows(), values.cols())
{
}

Dataset::Dataset(Eigen::Ref<Matrix const> ref)
    : variables(defaultVariables((size_t)ref.cols()))
    , map(ref.data(), ref.rows(), ref.cols())
{
}

void Dataset::SetVariableNames(std::vector<std::string> const& names)
{
    if (names.size() != (size_t)map.cols()) {
        auto msg = fmt::format("The number of columns ({}) does not match the number of column names ({}).", map.cols(), names.size());
        throw std::runtime_error(msg);
    }

    size_t ncol = (size_t)map.cols();

    for (size_t i = 0; i < ncol; ++i) {
        auto h = Hasher<HashFunction::XXHash>{}(reinterpret_cast<uint8_t const*>(names[i].c_str()), names[i].size());
        Variable v { names[i], h, i };
        variables[i] = v;
    }

    std::sort(variables.begin(), variables.end(), [&](auto& a, auto& b) { return a.Hash < b.Hash; });
}

std::vector<std::string> Dataset::VariableNames()
{
    std::vector<std::string> names;
    std::transform(variables.begin(), variables.end(), std::back_inserter(names), [](const Variable& v) { return v.Name; });
    return names;
}

Operon::Span<const Operon::Scalar> Dataset::GetValues(const std::string& name) const noexcept
{
    auto hashValue = Hasher<HashFunction::XXHash>{}(reinterpret_cast<uint8_t const*>(name.c_str()), name.size());
    return GetValues(hashValue);
}

Operon::Span<const Operon::Scalar> Dataset::GetValues(Operon::Hash hashValue) const noexcept
{
    auto it = std::partition_point(variables.begin(), variables.end(), [&](const auto& v) { return v.Hash < hashValue; });
    bool variable_exists_in_the_dataset = it != variables.end() && it->Hash == hashValue;
    ENSURE(variable_exists_in_the_dataset);
    auto idx = static_cast<Eigen::Index>(it->Index);
    return Operon::Span<const Operon::Scalar>(map.col(idx).data(), static_cast<size_t>(map.rows()));
}

// this method needs to take an int argument to differentiate it from GetValues(Operon::Hash)
Operon::Span<const Operon::Scalar> Dataset::GetValues(int index) const noexcept
{
    return Operon::Span<const Operon::Scalar>(map.col(index).data(), static_cast<size_t>(map.rows()));
}

std::optional<Variable> Dataset::GetVariable(const std::string& name) const noexcept
{
    auto hashValue = Hasher<HashFunction::XXHash>{}(reinterpret_cast<uint8_t const*>(name.c_str()), name.size());
    return GetVariable(hashValue);
}

std::optional<Variable> Dataset::GetVariable(Operon::Hash hashValue) const noexcept
{
    auto it = std::partition_point(variables.begin(), variables.end(), [&](const auto& v) { return v.Hash < hashValue; });
    return it != variables.end() && it->Hash == hashValue ? std::make_optional(*it) : std::nullopt;
}

void Dataset::Shuffle(Operon::RandomGenerator& random)
{
    if (IsView()) { throw std::runtime_error("Cannot shuffle. Dataset does not own the data.\n"); }
    Eigen::PermutationMatrix<Eigen::Dynamic, Eigen::Dynamic> perm(values.rows());
    perm.setIdentity();
    // generate a random permutation
    std::shuffle(perm.indices().data(), perm.indices().data() + perm.indices().size(), random);
    values = perm * values.matrix(); // permute rows
}

void Dataset::Normalize(size_t i, Range range)
{
    if (IsView()) { throw std::runtime_error("Cannot normalize. Dataset does not own the data.\n"); }
    EXPECT(range.Start() + range.Size() <= static_cast<size_t>(values.rows()));
    auto j     = static_cast<Eigen::Index>(i);
    auto start = static_cast<Eigen::Index>(range.Start());
    auto size  = static_cast<Eigen::Index>(range.Size());
    auto seg   = values.col(j).segment(start, size);
    auto min   = seg.minCoeff();
    auto max   = seg.maxCoeff();
    values.col(j) = (values.col(j).array() - min) / (max - min);
}

// standardize column i using mean and stddev calculated over the specified range
void Dataset::Standardize(size_t i, Range range)
{
    if (IsView()) { throw std::runtime_error("Cannot standardize. Dataset does not own the data.\n"); }
    EXPECT(range.Start() + range.Size() <= static_cast<size_t>(values.rows()));
    auto j = static_cast<Eigen::Index>(i);
    auto start = static_cast<Eigen::Index>(range.Start());
    auto n = static_cast<Eigen::Index>(range.Size());
    auto seg = values.col(j).segment(start, n);
    auto stats = univariate::accumulate<Matrix::Scalar>(seg.data(), seg.size());
    auto stddev = stats.variance * stats.variance;
    values.col(j) = (values.col(j).array() - stats.mean) / stddev;
}
} // namespace Operon
