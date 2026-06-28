module;

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>

export module AbslContainers;

export namespace absl
{
#ifndef ABSL_CONTAINERS
	using ::absl::flat_hash_map;
	using ::absl::flat_hash_set;
	using ::absl::btree_map;
	using ::absl::btree_set;
	using ::absl::btree_multimap;
	using ::absl::erase_if;
#endif
}
