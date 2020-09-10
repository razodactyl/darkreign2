#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/bitset.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/common.hpp>
#include <cereal/types/complex.hpp>
#include <cereal/types/deque.hpp>
#include <cereal/types/forward_list.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/queue.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/stack.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/unordered_set.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/valarray.hpp>
#include <cereal/types/vector.hpp>

const size_t constexpr const_hash(char const* input)
{
    return *input ? static_cast<size_t>(*input) + 33 * const_hash(input + 1) : 5381;
}

#define UniqueClassId_Declare(className) static const size_t constexpr UniqueClassId() { return const_hash(#className); }
#define UniqueClassId_ImplementInline(className) static const size_t constexpr UniqueClassId() { return const_hash(#className); }
