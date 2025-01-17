// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright 2019-2021 Heal Research

#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <optional>
#include <stack>
#include <utility>

#include "core/tree.hpp"

namespace Operon {
Tree& Tree::UpdateNodes()
{
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto& s = nodes[i];

        s.Depth = 1;
        s.Length = s.Arity;
        if (s.IsLeaf()) {
            continue;
        }
        auto j = i - 1;
        for (size_t k = 0; k < s.Arity; ++k) {
            auto &p = nodes[j];
            s.Length = static_cast<uint16_t>(s.Length + p.Length);
            s.Depth = std::max(s.Depth, p.Depth);
            p.Parent = static_cast<uint16_t>(i);
            j -= p.Length + 1;
        }
        ++s.Depth;
    }
    nodes.back().Level = 1;

    for (auto it = nodes.rbegin() + 1; it < nodes.rend(); ++it) {
        it->Level = static_cast<uint16_t>(nodes[it->Parent].Level + 1);
    }

    return *this;
}

Tree& Tree::Reduce()
{
    bool reduced = false;
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto& s = nodes[i];
        if (s.IsLeaf() || !s.IsCommutative()) {
            continue;
        }

        for (auto it = Children(i); it.HasNext(); ++it) {
            if (s.HashValue == it->HashValue) {
                it->IsEnabled = false;
                s.Arity = static_cast<uint16_t>(s.Arity + it->Arity - 1);
                reduced = true;
            }
        }
    }

    // if anything was reduced (nodes were disabled), copy remaining enabled nodes
    if (reduced) {
        // erase-remove idiom https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom
        nodes.erase(remove_if(nodes.begin(), nodes.end(), [](const Node& s) { return !s.IsEnabled; }), nodes.end());
    }
    // else, nothing to do
    return this->UpdateNodes();
}

// Sort each function node's children according to node type and hash value
// - note that entire child subtrees / subarrays are reordered inside the nodes array
// - this method assumes node hashes are computed, usually it is preceded by a call to tree.Hash()
Tree& Tree::Sort()
{
    // preallocate memory to reduce fragmentation
    Operon::Vector<Operon::Node> sorted = nodes;

    Operon::Vector<size_t> children;
    children.reserve(nodes.size());

    auto start = nodes.begin();

    for (uint16_t i = 0; i < nodes.size(); ++i) {
        auto& s = nodes[i];

        if (s.IsLeaf()) {
            continue;
        }

        auto arity = s.Arity;
        auto size = s.Length;

        if (s.IsCommutative()) {
            if (arity == size) {
                pdqsort(start + i - size, start + i);
            } else {
                for (auto it = Children(i); it.HasNext(); ++it) {
                    children.push_back(it.Index());
                }
                pdqsort(children.begin(), children.end(), [&](auto a, auto b) { return nodes[a] < nodes[b]; }); // sort child indices

                auto pos = sorted.begin() + i - size;
                for (auto j : children) {
                    auto& c = nodes[j];
                    std::copy_n(start + static_cast<long>(j) - c.Length, c.Length + 1, pos);
                    pos += c.Length + 1;
                }
                children.clear();
            }
        }
    }
    nodes.swap(sorted);
    return this->UpdateNodes();
}

std::vector<size_t> Tree::ChildIndices(size_t i) const
{
    if (nodes[i].IsLeaf()) {
        return std::vector<size_t> {};
    }
    std::vector<size_t> indices(nodes[i].Arity);
    size_t j = 0;
    for (auto it = Children(i); it.HasNext(); ++it) {
        indices[j++] = it.Index();
    }
    return indices;
}

std::vector<Operon::Scalar> Tree::GetCoefficients() const
{
    std::vector<Operon::Scalar> coefficients;
    for (auto const& s : nodes) {
        if (s.IsLeaf()) {
            coefficients.push_back(s.Value);
        }
    }
    return coefficients;
}

void Tree::SetCoefficients(const Operon::Span<const Operon::Scalar> coefficients)
{
    size_t idx = 0;
    for (auto& s : nodes) {
        if (s.IsLeaf()) {
            s.Value = static_cast<Operon::Scalar>(coefficients[idx++]);
        }
    }
}

size_t Tree::Depth() const noexcept
{
    return nodes.back().Depth;
}

size_t Tree::VisitationLength() const noexcept
{
    return std::transform_reduce(nodes.begin(), nodes.end(), 0UL, std::plus<> {}, [](const auto& node) { return node.Length + 1; });
}

Tree& Tree::Hash(Operon::HashFunction f, Operon::HashMode m)
{
    switch (f) {
    case Operon::HashFunction::XXHash: {
        return Hash<Operon::HashFunction::XXHash>(m);
    }
    case Operon::HashFunction::MetroHash: {
        return Hash<Operon::HashFunction::MetroHash>(m);
    }
    case Operon::HashFunction::FNV1Hash: {
        return Hash<Operon::HashFunction::FNV1Hash>(m);
    }
    }
    return *this;
}

} // namespace Operon
