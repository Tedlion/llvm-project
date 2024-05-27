/**
 * @brief
 * @authors tangwy
 * @date 2024/4/24
 */

#include "ExtendedODRHash.h"
#include "clang/AST/Attr.h"

void clang::class_wrapper::ExtendedODRHash::AddRecordDecl(
    const clang::RecordDecl *RD) {
/// What may be contained in a RecordDecl? Consider the following:
/// \code{.c}
/// struct S1{
///     struct S1 * ps1;    // ptr to self
///     struct S2 * ps2;    // ptr to another struct
///     struct S3 s3;  // another struct, which must be completely defined before
///     struct {
///         ...
///     } s4;  // struct defined nested in
/// };
/// \endcode

  for (const auto *Attr : RD->attrs()) {
    AddIdentifierInfo(Attr->getAttrName());
  }
  const Type *RecordType = RD->getTypeForDecl();

  if (Cache.contains(RecordType)){
    HashValue SubHash = Cache[RecordType];
    RecordHash.Hash = llvm::hash_combine(RecordHash.Value, SubHash.Value);
    if (!SubHash.Completed) {
      RecordHash.Completed = false;
    }
    return;
  }

  ODRHash::AddRecordDecl(RD);

  if (RecordType == TopDeclType) {
  } else if (Cache.contains(RecordType)) {
    HashValue SubHash = Cache[RecordType];
    RecordHash.Hash = llvm::hash_combine(RecordHash.Value, SubHash.Value);
    if (!SubHash.Completed) {
      RecordHash.Completed = false;
    }
  } else {
    RecordHash.Completed = false;
  }

//  for (const auto *Field : RD->fields()) {
//    QualType FieldType = Field->getType();
//    const Type *TypePtr = FieldType.getCanonicalType().getTypePtr();
//    TypePtr->dump();
//    if (TypePtr->isRecordType()) {
//      // Must have a complete definition before
//      assert(Cache.contains(TypePtr));
//      RecordHash.Hash = llvm::hash_combine(RecordHash.Value, Cache[TypePtr].Value);
//    }else if (TypePtr->isArrayType()){
//      do {
//        TypePtr = TypePtr->getArrayElementTypeNoTypeQual();
//      } while (TypePtr->isArrayType());
//      assert(Cache.contains(TypePtr));
//      RecordHash.Hash = llvm::hash_combine(RecordHash.Value, Cache[TypePtr].Value);
//    }
//    else if (TypePtr->isPointerType()){
//      do {
//        TypePtr = TypePtr->getPointeeType().getTypePtr();
//      } while (TypePtr->isPointerType());
//      // myself
//      // complete
//      // incomplete
//
//    }
//  }
}
