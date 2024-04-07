/**
 * @brief
 * @authors tangwy
 * @date 2024/4/7
 */

#include "FileFilter.h"

#include "llvm/Support/Path.h"

#include <algorithm>
#include <filesystem>

namespace llvm {
FileFilter::FileFilter(std::vector<std::string>::const_iterator FilesBegin,
                       std::vector<std::string>::const_iterator FilesEnd,
                       StringRef SrcRoot) {
  using std_path = std::filesystem::path;
  auto SrcRootPath = std_path(SrcRoot.str());
  if (FilesBegin == FilesEnd) {
    auto Pattern = GlobPattern::create((SrcRootPath / "*").string());
    if (Pattern){
      FilePathPatterns.push_back(std::make_pair(Inclusive, Pattern.get()));
    } else {
      llvm::errs() << "Invalid pattern: " << (SrcRootPath / "*").lexically_normal().generic_string() << "\n";
    }
    return;
  }

  for (auto I = FilesBegin; I != FilesEnd; ++I) {
    StringRef Path = *I;
    if (Path.empty())
      continue;
    MatchType Type = Inclusive;
    if (Path[0] == '+') {
      Path = Path.substr(1);
      Type = Inclusive;
    } else if (Path[0] == '-') {
      Path = Path.substr(1);
      Type = Exclusive;
    }
    std::string PathStr = llvm::sys::path::is_absolute(Path)
                              ? std_path(Path.str()).lexically_normal().generic_string()
                              : (SrcRootPath / Path.str()).lexically_normal().generic_string();
    auto Pattern = GlobPattern::create(PathStr);
    if (Pattern){
        FilePathPatterns.push_back(std::make_pair(Type, Pattern.get()));
            } else {
        llvm::errs() << "Invalid pattern: " << PathStr << "\n";
    }
  }
}

bool FileFilter::isMatched(llvm::StringRef Path) const {
  bool IsMatched = false;
  for (const auto &[matchType, pattern] : FilePathPatterns) {
    if (IsMatched && matchType == Inclusive) {
      continue;
    }
    if (!IsMatched && matchType == Exclusive) {
      continue;
    }
    auto NormalPath =
        std::filesystem::path(Path.str()).lexically_normal().generic_string();

    if (pattern.match(NormalPath)){
      IsMatched = !IsMatched;
    }
  }
  return IsMatched;
}

}; // namespace llvm