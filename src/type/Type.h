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

#pragma once

#include "Tree.h"

namespace nebula {
namespace type {

// All supported types in nebula
enum class Kind {
  INVALID = 0,
  BOOLEAN = 1,
  TINYINT = 2,
  SMALLINT = 3,
  INTEGER = 4,
  BIGINT = 5,
  REAL = 6,
  DOUBLE = 7,
  VARCHAR = 8,
  VARBINARY = 9,
  TIMESTAMP = 10,
  ARRAY = 11,
  MAP = 12,
  STRUCT = 13
};

/**
 * Define all individual type alias.
 * Some type has more aliases than others.
 */
template <Kind KIND>
class Type;
using BoolType = Type<Kind::BOOLEAN>;
using TinyType = Type<Kind::TINYINT>;
using ByteType = TinyType;
using SmallType = Type<Kind::SMALLINT>;
using ShortType = SmallType;
using IntType = Type<Kind::INTEGER>;
using BigType = Type<Kind::BIGINT>;
using LongType = BigType;
using RealType = Type<Kind::REAL>;
using FloatType = RealType;
using DoubleType = Type<Kind::DOUBLE>;
using VarcharType = Type<Kind::VARCHAR>;
using StringType = VarcharType;
using VarbinaryType = Type<Kind::VARBINARY>;
using BinaryType = VarbinaryType;
using TimestampType = Type<Kind::TIMESTAMP>;
using ArrayType = Type<Kind::ARRAY>;
using ListType = ArrayType;
using MapType = Type<Kind::MAP>;
using StructType = Type<Kind::STRUCT>;
using RowType = StructType;

/* 
Every type kind has different traits
- Kind const value 
- is primitive or compound type
- type value width: 0-variable length
- type name literals
*/
template <Kind KIND>
struct TypeTraits {
};

#define DEFINE_TYPE_TRAITS(NAME, PRIMITIVE, WIDTH) \
  template <>                                      \
  struct TypeTraits<Kind::NAME> {                  \
    static constexpr Kind typeKind = Kind::NAME;   \
    static constexpr bool isPrimitive = PRIMITIVE; \
    static constexpr size_t width = WIDTH;         \
    static constexpr auto name = #NAME;            \
  };

// define all traits for each KIND
DEFINE_TYPE_TRAITS(BOOLEAN, true, 1)
DEFINE_TYPE_TRAITS(TINYINT, true, 1)
DEFINE_TYPE_TRAITS(SMALLINT, true, 2)
DEFINE_TYPE_TRAITS(INTEGER, true, 4)
DEFINE_TYPE_TRAITS(BIGINT, true, 8)
DEFINE_TYPE_TRAITS(REAL, true, 4)
DEFINE_TYPE_TRAITS(DOUBLE, true, 8)
DEFINE_TYPE_TRAITS(VARCHAR, true, 0)
DEFINE_TYPE_TRAITS(VARBINARY, true, 0)
DEFINE_TYPE_TRAITS(TIMESTAMP, true, 8)
DEFINE_TYPE_TRAITS(ARRAY, false, 0)
DEFINE_TYPE_TRAITS(MAP, false, 0)
DEFINE_TYPE_TRAITS(STRUCT, false, 0)

#undef DEFINE_TYPE_TRAITS

// every type is templated by a KIND
// We need an abstract type to do generic operations
template <Kind KIND>
class Type : public Tree<Type<KIND>*> {
public:
  using TType = Type<KIND>;
  using PType = TType*;
  virtual ~Type() = default;

  // different KIND has different create method
  // primitive types
  // We put a default bool "OK" here so that it can be disabled for non-primitive types
  template <bool OK = true>
  static auto create(const std::string& name)
    -> typename std::enable_if<TypeTraits<KIND>::isPrimitive && OK, TType>::type {
    return TType(name);
  }

  // Array type
  template <Kind K, bool OK = true>
  static auto create(const std::string& name,
                     const std::shared_ptr<Type<K>>& child)
    -> typename std::enable_if<(TypeTraits<KIND>::typeKind == Kind::ARRAY) && OK, TType>::type {
    auto type = TType(name);
    auto childNode = std::static_pointer_cast<Tree<Type<K>*>>(child);
    const auto& added = type.addChild(childNode);

    // compare the address of these two object
    N_ENSURE_EQ(reinterpret_cast<std::uintptr_t>(added.value()),
                reinterpret_cast<std::uintptr_t>(child->value()),
                "same object with the same address");

    // copy elision
    return type;
  }

  // Map type
  template <Kind K, Kind V, bool OK = true>
  static auto create(const std::string& name,
                     const std::shared_ptr<Type<K>>& key,
                     const std::shared_ptr<Type<V>>& value)
    -> typename std::enable_if<(TypeTraits<KIND>::typeKind == Kind::MAP) && OK, TType>::type {
    auto type = TType(name);
    type.addChild(std::static_pointer_cast<Tree<Type<K>*>>(key));
    type.addChild(std::static_pointer_cast<Tree<Type<V>*>>(value));

    // compare the address of these two object
    N_ENSURE_EQ(type.size(), 2, "only 2 children allowed")

    // copy elision
    return type;
  }

  // Struct type (parameter pack)
  template <bool OK = TypeTraits<KIND>::typeKind == Kind::STRUCT, Kind... K>
  static auto create(const std::string& name, const std::shared_ptr<Type<K>>&... fields)
    -> typename std::enable_if<OK, TType>::type {
    auto type = TType(name);
    type.addChildren(std::static_pointer_cast<Tree<Type<K>*>>(fields)...);

    // pack expansion by sizeof
    N_ENSURE_EQ(type.size(), sizeof...(fields), "all children added");

    // add all fields
    return type;
  }

public:
  std::string
    toString() const {
    return fmt::format("[name={0}, width={1}]", name(), width());
  }

  Kind kind() const {
    return TypeTraits<KIND>::typeKind;
  }

  std::string type() const {
    return TypeTraits<KIND>::name;
  }

  bool isPrimitive() const {
    return TypeTraits<KIND>::isPrimitive;
  }

  bool isFixedWidth() const {
    return TypeTraits<KIND>::width > 0;
  }

  size_t width() const {
    return TypeTraits<KIND>::width;
  }

  std::string name() const {
    return name_;
  }

protected:
  Type(std::string name) : name_{ name }, Tree<PType>(this) {
    LOG(INFO) << "Construct a type" << toString();
  }

protected:
  std::string name_;
};

} // namespace type
} // namespace nebula