/**
 * @brief
 * @authors tangwy
 * @date 2024/4/23
 */

#ifndef SUPPORT_H
#define SUPPORT_H

#include "llvm/ADT/StringRef.h"
#include <format>

template <>
struct std::formatter<llvm::StringRef> : std::formatter<std::string_view> {
  auto format(llvm::StringRef S, std::format_context &Ctx) const {
    return std::formatter<std::string_view>::format((std::string_view)S, Ctx);
  }
};

#endif // SUPPORT_H
