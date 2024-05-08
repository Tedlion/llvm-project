/**
 * @brief
 * @authors tangwy
 * @date 2024/4/24
 */

#include "ExtendedODRHash.h"
#include "clang/AST/Attr.h"

void clang::class_wrapper::ExtendedODRHash::AddRecordDecl(
    const clang::RecordDecl *RD) {
  // FIXME: endless recursive!
  ODRHash::AddRecordDecl(RD);
  for (const auto *Attr : RD->attrs()) {
    AddIdentifierInfo(Attr->getAttrName());
  }
  const Type *RecordType = RD->getTypeForDecl();
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
