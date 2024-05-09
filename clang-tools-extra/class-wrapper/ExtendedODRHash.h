/**
 * @brief
 * @authors tangwy
 * @date 2024/4/24
 */

#ifndef EXTENDEDODRHASH_H
#define EXTENDEDODRHASH_H
#include "clang/AST/Decl.h"
#include "clang/AST/ODRHash.h"
#include "llvm/ADT/Hashing.h"
//===-- ODRHash.h - Hashing to diagnose ODR failures ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// The clang::ODRHash class, which calculates a hash based on AST nodes,
/// is able to distinguish AST structures compiled with different options.
/// However, it fails to consider the following cases:
/// 1. Nested RecordDecl, i.e., struct used in other Decls.
/// For example, struct s1 is used in struct s2 in following code:
/// \code{.c}
///struct s1{
///#if EXTRA_MACRO
///   char x;
///#endif
///   unsigned dwMsgLen;
///};
///
///struct s2{
///   struct s1 s_1;
///};
///struct s3{
///    struct * s1 s_1;
///}
///\endcode
/// When compling the code with and without the EXTRA_MACRO defined, the struct
/// s2 is different since it contains different struct s1.
/// However, the clang::ORDHash only consider the type name "struct s1", it does
/// not recursively takes the hash value of struct s1.
///
/// In our class wrapper design, if a type s1 is wrapped in target namespace,
/// then any type/function relay on the type s1 must be in the target namespace,
/// even when the type s1 is used as a pointer.
///
//===----------------------------------------------------------------------===//

// FIXME: Incomplete type pointers, and may point to each other?
//  Is that a rare case?
//  Treat all inconsistent?
//  Or, return a vector of symbols which depends on.

namespace clang::class_wrapper {

class ExtendedODRHash : private ODRHash {
public:
  union HashValue {
    struct {
      unsigned long long Valid : 1;
      unsigned long long Completed : 1;
      unsigned long long Hash : 62;
    };
    unsigned long long Value;

    operator bool() const { return Valid; }

    bool operator==(const HashValue &Other) const {
      return !Valid && !Other.Valid || Value == Other.Value;
    }
  };
  using ODRHashCache = llvm::DenseMap<const Type *, HashValue>;
  constexpr static HashValue HashValueInvalid = HashValue{};

private:
  ODRHashCache &Cache; // supposed to be clear after each translation unit.

  constexpr static HashValue InitHashValue = {true, true, 0x202404251447};
  HashValue RecordHash = InitHashValue;
  const Type * TopDeclType = nullptr;

public:
  ExtendedODRHash(ODRHashCache &Cache) : Cache(Cache) {}

  HashValue CalculateHash() {
    unsigned ODRHash = ODRHash::CalculateHash();
    return {true, RecordHash.Completed,
            llvm::hash_combine(ODRHash, RecordHash.Value)};
  }

  void clear() {
    ODRHash::clear();
    RecordHash = InitHashValue;
    TopDeclType = nullptr;
  }

  /// The hash value will also be recorded in Cache.
  static HashValue calculateRecordDeclHash(const RecordDecl *RD,
                                           ODRHashCache &Cache) {
    const Type *T = RD->getTypeForDecl();
    if (Cache.contains(T)) {
      return Cache[T];
    }
    ExtendedODRHash Hash(Cache);
    Hash.TopDeclType = T;
    Hash.AddRecordDecl(RD);
    HashValue Value = Hash.CalculateHash();
    if (RD->isCompleteDefinition()) {
      Cache[T] = Value;
    }
    return Value;
  }

private:
  void AddRecordDecl(const RecordDecl *Record) override;

  void AddDecl(const Decl *D) override {
    if (const RecordDecl *RD = dyn_cast<RecordDecl>(D)) {
      AddRecordDecl(RD);
    } else {
      ODRHash::AddDecl(D);
    }
  }

};

} // namespace clang::class_wrapper

#endif // EXTENDEDODRHASH_H
