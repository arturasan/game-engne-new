#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace engine {

template <typename T> class Events {
public:
    void send(T event) {
        const auto id = next_id_++;
        current_.push_back(Record{id, std::move(event)});
        readable_.push_back(current_.back().value);
    }

    [[nodiscard]] std::span<const T> read() const noexcept {
        return readable_;
    }

    void update() {
        previous_ = std::move(current_);
        current_.clear();
        readable_.clear();
        readable_.reserve(previous_.size());
        for (const auto& event : previous_) {
            readable_.push_back(event.value);
        }
    }

    template <typename Output> void read_since(std::uint64_t& cursor, Output& output) const {
        output.clear();
        auto newest = cursor;
        append_since(previous_, cursor, newest, output);
        append_since(current_, cursor, newest, output);
        cursor = newest;
    }

private:
    struct Record {
        std::uint64_t id = 0;
        T value;
    };

    template <typename Output>
    static void append_since(const std::vector<Record>& records, std::uint64_t cursor,
                             std::uint64_t& newest, Output& output) {
        for (const auto& event : records) {
            if (event.id <= cursor) {
                continue;
            }
            output.push_back(event.value);
            newest = event.id;
        }
    }

    std::vector<Record> previous_;
    std::vector<Record> current_;
    std::vector<T> readable_;
    std::uint64_t next_id_ = 1;
};

template <typename T> class Res {
public:
    explicit Res(const T& resource) noexcept : resource_(&resource) {}

    [[nodiscard]] const T& get() const noexcept {
        return *resource_;
    }

    [[nodiscard]] const T& operator*() const noexcept {
        return get();
    }

    [[nodiscard]] const T* operator->() const noexcept {
        return resource_;
    }

private:
    const T* resource_;
};

template <typename T> class ResMut {
public:
    explicit ResMut(T& resource) noexcept : resource_(&resource) {}

    [[nodiscard]] T& get() const noexcept {
        return *resource_;
    }

    [[nodiscard]] T& operator*() const noexcept {
        return get();
    }

    [[nodiscard]] T* operator->() const noexcept {
        return resource_;
    }

private:
    T* resource_;
};

template <typename T> class EventReader {
public:
    EventReader(const Events<T>& events, std::uint64_t& cursor, std::vector<T>& unread) noexcept
        : events_(&events), cursor_(&cursor), unread_(&unread) {}

    [[nodiscard]] std::span<const T> read() const {
        if (!read_) {
            events_->read_since(*cursor_, *unread_);
            read_ = true;
        }
        return *unread_;
    }

private:
    const Events<T>* events_;
    std::uint64_t* cursor_;
    std::vector<T>* unread_;
    mutable bool read_ = false;
};

template <typename T> class EventWriter {
public:
    explicit EventWriter(Events<T>& events) noexcept : events_(&events) {}

    void send(T event) const {
        events_->send(std::move(event));
    }

private:
    Events<T>* events_;
};

} // namespace engine
