// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

// Interface for the CPU hashmap. Separated from HashmapCPU.hpp for brevity.

#include <unordered_map>
#include "Open3D/Core/Hashmap/HashmapBase.h"
#include "Open3D/Core/Hashmap/Traits.h"

namespace open3d {

template <typename Hash, typename KeyEq>
class CPUHashmap : public Hashmap<Hash, KeyEq> {
public:
    ~CPUHashmap();

    CPUHashmap(uint32_t init_buckets,
               uint32_t dsize_key,
               uint32_t dsize_value,
               Device device);

    /// TODO: implement CPU counterpart here
    void Rehash(uint32_t buckets){};

    std::pair<iterator_t*, uint8_t*> Insert(uint8_t* input_keys,
                                            uint8_t* input_values,
                                            uint32_t input_key_size);

    std::pair<iterator_t*, uint8_t*> Find(uint8_t* input_keys,
                                          uint32_t input_key_size);

    uint8_t* Erase(uint8_t* input_keys, uint32_t input_key_size);

    /// TODO: replace place-holder
    std::pair<iterator_t*, uint32_t> GetIterators() {
        iterator_t* iterators = nullptr;
        uint32_t num_iterators = 0;
        return std::make_pair(iterators, num_iterators);
    }

    void UnpackIterators(iterator_t* input_iterators,
                         uint8_t* input_masks,
                         uint8_t* output_keys,
                         uint8_t* output_values,
                         uint32_t iterator_count) {}

    void AssignIterators(iterator_t* input_iterators,
                         uint8_t* input_masks,
                         uint8_t* input_values,
                         uint32_t iterator_count) {}

private:
    std::shared_ptr<std::unordered_map<uint8_t*, uint8_t*, Hash, KeyEq>>
            cpu_hashmap_impl_;

    // Valid kv_pairs
    std::vector<iterator_t> kv_pairs_;
};
}  // namespace open3d