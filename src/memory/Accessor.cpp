/*
 * Copyright 2017-present Shawn Cao
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <numeric>
#include "Batch.h"
#include "common/Errors.h"

namespace nebula {
namespace memory {

using nebula::meta::BessType;
using nebula::surface::ListData;
using nebula::surface::MapData;

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////// ROW Accessor ///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
RowAccessor& RowAccessor::seek(size_t rowId) {
  // seek to row ID and return myself
  // TODO(cao) - a runtime ephemeral/transient state like this is bad for parallelism
  // we'd better to move it to API itself though it looks a bit more complex.
  N_ENSURE(rowId < batch_.rows_, "row id out of bound");
  current_ = rowId;

  // populate all dimension values encoded in bess
  if (batch_.pod_ != nullptr) {
    bessValue_ = batch_.bess_.readBits(current_ * batch_.bessBits_, batch_.bessBits_);
  }

  return *this;
}

bool RowAccessor::isNull(const std::string& field) const {
  // check if field existing
  const auto& itr = dnMap_.find(field);
  N_ENSURE(itr != dnMap_.end(), fmt::format("field {0} not found!", field));
  return itr->second->isNull(current_);
}

#define READ_TYPE_BY_FIELD(TYPE, FUNC)                                                        \
  TYPE RowAccessor::FUNC(const std::string& field) const {                                    \
    TYPE v;                                                                                   \
    if (batch_.pod_ != nullptr && batch_.pod_->value(field, batch_.spaces_, bessValue_, v)) { \
      return v;                                                                               \
    }                                                                                         \
    return dnMap_.at(field)->read<TYPE>(current_);                                            \
  }

READ_TYPE_BY_FIELD(bool, readBool)
READ_TYPE_BY_FIELD(int8_t, readByte)
READ_TYPE_BY_FIELD(int16_t, readShort)
READ_TYPE_BY_FIELD(int32_t, readInt)
READ_TYPE_BY_FIELD(int64_t, readLong)
READ_TYPE_BY_FIELD(float, readFloat)
READ_TYPE_BY_FIELD(double, readDouble)
READ_TYPE_BY_FIELD(int128_t, readInt128)
READ_TYPE_BY_FIELD(std::string_view, readString)

#undef READ_TYPE_BY_FIELD

// compound types
// TODO(cao) - return a unique ptr seems unncessary expensive to create list accessor object every time
// we may want to maintain single instance and return a reference instead
std::unique_ptr<ListData> RowAccessor::readList(const std::string& field) const {
  // initilize a list data with child data node with
  auto listNode = dnMap_.at(field);

  // list node has only one child - can be saved if list accessor is created once
  auto child = listNode->childAt<PDataNode>(0).value();
  auto os = listNode->offsetSize(current_);

  // calculate the offset in terms of number of items
  return std::make_unique<ListAccessor>(os.first, os.second, child);
}

std::unique_ptr<MapData> RowAccessor::readMap(const std::string&) const {
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////// List Accessor //////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

inline bool ListAccessor::isNull(IndexType index) const {
  return node_->isNull(index);
}

#define READ_TYPE_BY_ENTRY(TYPE, FUNC)             \
  TYPE ListAccessor::FUNC(IndexType index) const { \
    return node_->read<TYPE>(offset_ + index);     \
  }

READ_TYPE_BY_ENTRY(bool, readBool)
READ_TYPE_BY_ENTRY(int8_t, readByte)
READ_TYPE_BY_ENTRY(int16_t, readShort)
READ_TYPE_BY_ENTRY(int32_t, readInt)
READ_TYPE_BY_ENTRY(int64_t, readLong)
READ_TYPE_BY_ENTRY(float, readFloat)
READ_TYPE_BY_ENTRY(double, readDouble)
READ_TYPE_BY_ENTRY(int128_t, readInt128)
READ_TYPE_BY_ENTRY(std::string_view, readString)

#undef READ_TYPE_BY_ENTRY

} // namespace memory
} // namespace nebula