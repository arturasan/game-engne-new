#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/ecs/detail/archetype.hpp"
#include "engine/ecs/entity.hpp"

namespace engine {

class World {
public:
    World() {
        static_cast<void>(get_or_create_archetype({}));
    }

    Entity spawn() {
        Entity entity{};
        if (!free_slots_.empty()) {
            entity.id = free_slots_.back();
            free_slots_.pop_back();
        } else {
            entity.id = static_cast<std::uint32_t>(slots_.size());
            slots_.push_back(Slot{});
        }

        Slot& slot = slots_[entity.id];
        entity.generation = slot.generation;
        slot.archetype = empty_archetype_;
        auto& archetype = *archetypes_[empty_archetype_];
        slot.row = static_cast<std::uint32_t>(archetype.entities.size());
        archetype.entities.push_back(entity);
        return entity;
    }

    template <typename... Cs> Entity spawn(Cs&&... components) {
        static_assert(
            detail::unique_component_types<Cs...>,
            "World::spawn<Ts...>() requires unique component types after removing cv/ref");

        Entity entity{};
        if (!free_slots_.empty()) {
            entity.id = free_slots_.back();
            free_slots_.pop_back();
        } else {
            entity.id = static_cast<std::uint32_t>(slots_.size());
            slots_.push_back(Slot{});
        }

        static const std::vector<ComponentId> signature = sorted_signature<Cs...>();
        const std::uint32_t archetype_index = get_or_create_archetype(signature);
        auto& archetype = *archetypes_[archetype_index];
        Slot& slot = slots_[entity.id];
        entity.generation = slot.generation;
        slot.archetype = archetype_index;
        slot.row = static_cast<std::uint32_t>(archetype.entities.size());
        archetype.entities.push_back(entity);
        (emplace_component<std::remove_cvref_t<Cs>>(archetype, std::forward<Cs>(components)), ...);
        return entity;
    }

    bool despawn(Entity entity) {
        Slot* slot = live_slot(entity);
        if (slot == nullptr) {
            return false;
        }

        auto& archetype = *archetypes_[slot->archetype];
        const auto moved = archetype.erase_row_swap(slot->row);
        if (moved.has_value()) {
            slots_[moved->id].row = slot->row;
        }

        slot->archetype = detail::invalid_archetype;
        slot->row = detail::invalid_row;
        ++slot->generation;
        free_slots_.push_back(entity.id);
        return true;
    }

    [[nodiscard]] bool alive(Entity entity) const {
        return live_slot(entity) != nullptr;
    }

    template <typename T, typename... Args>
    std::remove_cv_t<T>* add(Entity entity, Args&&... args) {
        using Raw = std::remove_cv_t<T>;
        Slot* slot = live_slot(entity);
        if (slot == nullptr) {
            return nullptr;
        }

        const ComponentId id = component_id_for<Raw>();
        auto& source = *archetypes_[slot->archetype];
        if (auto* existing = typed_column<Raw>(source, id)) {
            existing->value(slot->row) = Raw(std::forward<Args>(args)...);
            return &existing->value(slot->row);
        }

        const std::uint32_t destination_index = add_edge(slot->archetype, id);
        auto& destination = *archetypes_[destination_index];
        const std::uint32_t destination_row =
            static_cast<std::uint32_t>(destination.entities.size());
        destination.entities.push_back(entity);

        for (const ComponentId destination_id : destination.signature) {
            auto* destination_column = destination.column_for(destination_id);
            if (destination_id == id) {
                auto* typed_destination =
                    static_cast<detail::TypedComponentColumn<Raw>*>(destination_column);
                typed_destination->emplace_back(std::forward<Args>(args)...);
            } else {
                auto* source_column = source.column_for(destination_id);
                destination_column->move_push_from(*source_column, slot->row);
            }
        }

        const auto moved = source.erase_row_swap(slot->row);
        if (moved.has_value()) {
            slots_[moved->id].row = slot->row;
        }

        slot->archetype = destination_index;
        slot->row = destination_row;
        return get<Raw>(entity);
    }

    template <typename T> bool remove(Entity entity) {
        using Raw = std::remove_cv_t<T>;
        Slot* slot = live_slot(entity);
        if (slot == nullptr) {
            return false;
        }

        const ComponentId id = component_id_for<Raw>();
        auto& source = *archetypes_[slot->archetype];
        if (!source.has(id)) {
            return false;
        }

        const std::uint32_t destination_index = remove_edge(slot->archetype, id);
        auto& destination = *archetypes_[destination_index];
        const std::uint32_t destination_row =
            static_cast<std::uint32_t>(destination.entities.size());
        destination.entities.push_back(entity);

        for (const ComponentId destination_id : destination.signature) {
            auto* destination_column = destination.column_for(destination_id);
            auto* source_column = source.column_for(destination_id);
            destination_column->move_push_from(*source_column, slot->row);
        }

        const auto moved = source.erase_row_swap(slot->row);
        if (moved.has_value()) {
            slots_[moved->id].row = slot->row;
        }

        slot->archetype = destination_index;
        slot->row = destination_row;
        return true;
    }

    template <typename T> [[nodiscard]] std::remove_cv_t<T>* get(Entity entity) {
        using Raw = std::remove_cv_t<T>;
        Slot* slot = live_slot(entity);
        if (slot == nullptr) {
            return nullptr;
        }
        auto& archetype = *archetypes_[slot->archetype];
        auto* column = typed_column<Raw>(archetype, component_id_for<Raw>());
        if (column == nullptr) {
            return nullptr;
        }
        return &column->value(slot->row);
    }

    template <typename T> [[nodiscard]] const std::remove_cv_t<T>* get(Entity entity) const {
        using Raw = std::remove_cv_t<T>;
        const Slot* slot = live_slot(entity);
        if (slot == nullptr) {
            return nullptr;
        }
        const auto& archetype = *archetypes_[slot->archetype];
        auto* column = typed_column<Raw>(archetype, component_id_for<Raw>());
        if (column == nullptr) {
            return nullptr;
        }
        return &column->value(slot->row);
    }

    template <typename T> [[nodiscard]] bool has(Entity entity) const {
        return get<T>(entity) != nullptr;
    }

    template <typename... Cs, typename Fn> void for_each(Fn&& fn) {
        static_assert(
            detail::unique_component_types<Cs...>,
            "World::for_each<Ts...>() requires unique component types after removing cv/ref");

        static const std::vector<ComponentId> signature = sorted_signature<Cs...>();
        for (auto& archetype_ptr : archetypes_) {
            auto& archetype = *archetype_ptr;
            if (!archetype.contains_all(signature)) {
                continue;
            }
            auto columns = std::tuple{
                typed_column<std::remove_cv_t<Cs>>(archetype, component_id_for<Cs>())...};
            for (std::size_t row = 0; row < archetype.entities.size(); ++row) {
                std::invoke(
                    fn, archetype.entities[row],
                    std::get<detail::TypedComponentColumn<std::remove_cv_t<Cs>>*>(columns)->value(
                        row)...);
            }
        }
    }

    template <typename... Cs, typename Fn> void for_each(Fn&& fn) const {
        static_assert(
            detail::unique_component_types<Cs...>,
            "World::for_each<Ts...>() requires unique component types after removing cv/ref");

        static const std::vector<ComponentId> signature = sorted_signature<Cs...>();
        for (const auto& archetype_ptr : archetypes_) {
            const auto& archetype = *archetype_ptr;
            if (!archetype.contains_all(signature)) {
                continue;
            }
            auto columns = std::tuple{
                typed_column<std::remove_cv_t<Cs>>(archetype, component_id_for<Cs>())...};
            for (std::size_t row = 0; row < archetype.entities.size(); ++row) {
                std::invoke(
                    fn, archetype.entities[row],
                    std::get<const detail::TypedComponentColumn<std::remove_cv_t<Cs>>*>(columns)
                        ->value(row)...);
            }
        }
    }

private:
    struct Slot {
        std::uint32_t generation = 0;
        std::uint32_t archetype = detail::invalid_archetype;
        std::uint32_t row = detail::invalid_row;
    };

    [[nodiscard]] Slot* live_slot(Entity entity) {
        if (entity.id >= slots_.size()) {
            return nullptr;
        }
        Slot& slot = slots_[entity.id];
        if (slot.generation != entity.generation || slot.archetype == detail::invalid_archetype) {
            return nullptr;
        }
        return &slot;
    }

    [[nodiscard]] const Slot* live_slot(Entity entity) const {
        if (entity.id >= slots_.size()) {
            return nullptr;
        }
        const Slot& slot = slots_[entity.id];
        if (slot.generation != entity.generation || slot.archetype == detail::invalid_archetype) {
            return nullptr;
        }
        return &slot;
    }

    template <typename... Cs> [[nodiscard]] static std::vector<ComponentId> sorted_signature() {
        std::vector<ComponentId> ids{component_id_for<std::remove_cv_t<Cs>>()...};
        std::ranges::sort(ids);
        return ids;
    }

    template <typename T>
    [[nodiscard]] static detail::TypedComponentColumn<T>* typed_column(detail::Archetype& archetype,
                                                                       ComponentId id) {
        return static_cast<detail::TypedComponentColumn<T>*>(archetype.column_for(id));
    }

    template <typename T>
    [[nodiscard]] static const detail::TypedComponentColumn<T>*
    typed_column(const detail::Archetype& archetype, ComponentId id) {
        return static_cast<const detail::TypedComponentColumn<T>*>(archetype.column_for(id));
    }

    template <typename T, typename Value>
    static void emplace_component(detail::Archetype& archetype, Value&& value) {
        auto* column = typed_column<T>(archetype, component_id_for<T>());
        column->emplace_back(std::forward<Value>(value));
    }

    [[nodiscard]] std::uint32_t add_edge(std::uint32_t source_index, ComponentId id) {
        auto& source = *archetypes_[source_index];
        if (const auto it = source.add_edges.find(id); it != source.add_edges.end()) {
            return it->second;
        }

        std::vector<ComponentId> destination_signature = source.signature;
        destination_signature.push_back(id);
        std::ranges::sort(destination_signature);
        const std::uint32_t destination_index = get_or_create_archetype(destination_signature);
        source.add_edges.emplace(id, destination_index);
        archetypes_[destination_index]->remove_edges.emplace(id, source_index);
        return destination_index;
    }

    [[nodiscard]] std::uint32_t remove_edge(std::uint32_t source_index, ComponentId id) {
        auto& source = *archetypes_[source_index];
        if (const auto it = source.remove_edges.find(id); it != source.remove_edges.end()) {
            return it->second;
        }

        std::vector<ComponentId> destination_signature;
        destination_signature.reserve(source.signature.size() - 1U);
        for (const ComponentId current_id : source.signature) {
            if (current_id != id) {
                destination_signature.push_back(current_id);
            }
        }
        const std::uint32_t destination_index = get_or_create_archetype(destination_signature);
        source.remove_edges.emplace(id, destination_index);
        archetypes_[destination_index]->add_edges.emplace(id, source_index);
        return destination_index;
    }

    [[nodiscard]] std::uint32_t get_or_create_archetype(const std::vector<ComponentId>& signature) {
        if (const auto it = archetype_lookup_.find(signature); it != archetype_lookup_.end()) {
            return it->second;
        }

        const std::uint32_t index = static_cast<std::uint32_t>(archetypes_.size());
        auto archetype = std::make_unique<detail::Archetype>(std::vector<ComponentId>{signature});
        for (const ComponentId id : archetype->signature) {
            archetype->add_column(id, detail::component_info_for_id(id).make_column());
        }
        archetype_lookup_.emplace(archetype->signature, index);
        archetypes_.push_back(std::move(archetype));
        if (archetypes_[index]->signature.empty()) {
            empty_archetype_ = index;
        }
        return index;
    }

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<std::unique_ptr<detail::Archetype>> archetypes_;
    std::unordered_map<std::vector<ComponentId>, std::uint32_t, detail::SignatureHash>
        archetype_lookup_;
    std::uint32_t empty_archetype_ = detail::invalid_archetype;
};

} // namespace engine
