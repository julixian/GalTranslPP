module;

#include "minihash.hpp"

export module Exporter;

import std;

export namespace repro {
    using Cache = TinyMap<std::string, int>;

    inline int lookup(const Cache& cache) {
        if (auto it = cache.find("k"); it != cache.end()) {
            return it->second;
        }
        return 0;
    }
}
