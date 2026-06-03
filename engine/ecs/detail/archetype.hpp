#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "engine/ecs/detail/component_id.hpp"
#include "engine/ecs/entity.hpp"

namespace engine::detail {

inline constexpr std::uint32_t invalid_archetype = ~std::uint32_t{0};
inline constexpr std::uint32_t invalid_row = ~std::uint32_t{0};

struct SignatureHash {
    [[nodiscard]] std::size_t operator()(const std::vector<ComponentId>& ids) const noexcept {
        std::size_t hash = ids.size();
        for (const ComponentId id : ids) {
            hash ^=
                static_cast<std::size_t>(id) + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
        }
        return hash;
    }
};

struct Archetype {
    explicit Archetype(std::vector<ComponentId> ids) : signature(std::move(ids)) {}

    [[nodiscard]] bool has(ComponentId id) const {
        return column_indices.contains(id);
    }

    [[nodiscard]] bool contains_all(const std::vector<ComponentId>& ids) const {
        return std::ranges::includes(signature, ids);
    }

    [[nodiscard]] ComponentColumn* column_for(ComponentId id) {
        const auto it = column_indices.find(id);
        if (it == column_indices.end()) {
            return nullptr;
        }
        return columns[it->second].get();
    }

    [[nodiscard]] const ComponentColumn* column_for(ComponentId id) const {
        const auto it = column_indices.find(id);
        if (it == column_indices.end()) {
            return nullptr;
        }
        return columns[it->second].get();
    }

    void add_column(ComponentId id, std::unique_ptr<ComponentColumn> column) {
        column_indices.emplace(id, columns.size());
        columns.push_back(std::move(column));
    }

    [[nodiscard]] std::optional<Entity> erase_row_swap(std::size_t row) {
        std::optional<Entity> moved;
        if (row + 1U != entities.size()) {
            moved = entities.back();
            entities[row] = *moved;
        }
        for (auto& column : columns) {
            column->erase_swap(row);
        }
        entities.pop_back();
        return moved;
    }

    std::vector<ComponentId> signature;
    std::unordered_map<ComponentId, std::size_t> column_indices;
    std::vector<std::unique_ptr<ComponentColumn>> columns;
    std::vector<Entity> entities;
    std::unordered_map<ComponentId, std::uint32_t> add_edges;
    std::unordered_map<ComponentId, std::uint32_t> remove_edges;
};

} // namespace engine::detail
