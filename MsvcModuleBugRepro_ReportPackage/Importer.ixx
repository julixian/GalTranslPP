module;

#include "minihash.hpp"

export module Importer;

import std;
import Exporter;

export namespace repro {
    inline int imported_lookup(const Cache& cache) {
        return lookup(cache);
    }
}
