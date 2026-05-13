#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

namespace repro {
    class PiecewiseCombiner {
    public:
        PiecewiseCombiner() = default;
        PiecewiseCombiner(const PiecewiseCombiner&) = delete;
        PiecewiseCombiner& operator=(const PiecewiseCombiner&) = delete;

        template <typename H>
        H add_buffer(H state, const unsigned char* data, std::size_t size);

        template <typename H>
        H add_buffer(H state, const char* data, std::size_t size) {
            return add_buffer(std::move(state), reinterpret_cast<const unsigned char*>(data), size);
        }

        template <typename H>
        H finalize(H state);

    private:
        unsigned char buf_[1024]{};
        std::size_t position_ = 0;
        bool added_something_ = false;
    };

    template <typename Derived>
    class HashStateBase {
    public:
        template <typename T>
        static Derived combine(Derived state, const T& value) {
            return AbslHashValue(std::move(state), value);
        }
    };

    class MixingHashState : public HashStateBase<MixingHashState> {
    public:
        MixingHashState(MixingHashState&&) = default;
        MixingHashState& operator=(MixingHashState&&) = default;

        static MixingHashState combine_contiguous(MixingHashState state, const unsigned char* data, std::size_t size) {
            auto mixed = state.state_;
            for (std::size_t i = 0; i < size; ++i) {
                mixed = mixed * 131 + data[i];
            }
            return MixingHashState(mixed);
        }

        using MixingHashState::HashStateBase::combine;
        using AbslInternalPiecewiseCombiner = PiecewiseCombiner;

        template <typename T>
        static std::size_t hash(const T& value) {
            return combine(MixingHashState{}, value).state_;
        }

    private:
        friend class HashStateBase<MixingHashState>;
        MixingHashState() = default;
        explicit MixingHashState(std::size_t state) : state_(state) {}

        std::size_t state_ = 0;
    };

    template <typename H>
    H PiecewiseCombiner::add_buffer(H state, const unsigned char* data, std::size_t size) {
        if (position_ + size < sizeof(buf_)) {
            std::memcpy(buf_ + position_, data, size);
            position_ += size;
            return state;
        }
        added_something_ = true;
        state = H::combine_contiguous(std::move(state), buf_, position_);
        position_ = 0;
        return H::combine_contiguous(std::move(state), data, size);
    }

    template <typename H>
    H PiecewiseCombiner::finalize(H state) {
        if (added_something_ && position_ == 0) {
            return state;
        }
        return H::combine_contiguous(std::move(state), buf_, position_);
    }

    template <typename H>
    H AbslHashValue(H state, const std::string& value) {
        typename H::AbslInternalPiecewiseCombiner combiner;
        state = combiner.add_buffer(std::move(state), value.data(), value.size());
        return combiner.finalize(std::move(state));
    }

    template <typename K, typename V>
    class TinyMap {
    public:
        struct Entry {
            K first;
            V second;
        };

        using value_type = Entry;

        value_type* find(const K& key) {
            (void)MixingHashState::hash(key);
            if (has_value_ && entry_.first == key) {
                return &entry_;
            }
            return nullptr;
        }

        const value_type* find(const K& key) const {
            (void)MixingHashState::hash(key);
            if (has_value_ && entry_.first == key) {
                return &entry_;
            }
            return nullptr;
        }

        const value_type* end() const {
            return nullptr;
        }

        void emplace(K key, V value) {
            entry_ = value_type{ std::move(key), std::move(value) };
            has_value_ = true;
        }

    private:
        value_type entry_{};
        bool has_value_ = false;
    };
}
