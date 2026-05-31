#include "FileFilter.h"

#include "llvm/Support/Path.h"

#include <algorithm>
#include <filesystem>

namespace llvm {
std::string pathNormalize(const std::string &Path) {
  return std::filesystem::path(Path).lexically_normal().generic_string();
}

Expected<OwnedGlobPattern> OwnedGlobPattern::create(StringRef PatternText) {
  auto PatternStorage = std::make_unique<char[]>(PatternText.size());
  std::copy_n(PatternText.data(), PatternText.size(), PatternStorage.get());

  auto Pattern = GlobPattern::create(
    StringRef(PatternStorage.get(), PatternText.size()));
  if (!Pattern)
    return Pattern.takeError();

  return OwnedGlobPattern(std::move(PatternStorage), std::move(*Pattern));
}


FileFilter::FileFilter(std::vector<std::string>::const_iterator RuleBegin,
                       std::vector<std::string>::const_iterator RuleEnd,
                       StringRef SrcRoot) {
  using std_path = std::filesystem::path;
  auto SrcRootPath = std_path(SrcRoot.str());
  if (RuleBegin == RuleEnd) {
    std::string PatternText = (SrcRootPath / "*").lexically_normal().generic_string();
    auto Pattern = OwnedGlobPattern::create(PatternText);
    if (Pattern) {
      FilePathPatterns.emplace_back(Inclusive, std::move(*Pattern));
    } else {
      llvm::errs() << "Invalid pattern: " << (SrcRootPath / "*").
          lexically_normal().generic_string() << "\n";
    }
    return;
  }

  for (auto I = RuleBegin; I != RuleEnd; ++I) {
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
    std::string PathStr = sys::path::is_absolute(Path)
                            ? std_path(Path.str()).lexically_normal().
                            generic_string()
                            : (SrcRootPath / Path.str()).lexically_normal().
                            generic_string();
    auto Pattern = OwnedGlobPattern::create(PathStr);
    if (Pattern) {
      FilePathPatterns.emplace_back(Type, std::move(*Pattern));
    } else {
      llvm::errs() << "Invalid pattern: " << PathStr << "\n";
    }
  }
}

bool FileFilter::isMatched(llvm::StringRef Path) const {
  bool IsMatched = false;
  for (const auto &[MatchType, Pattern]: FilePathPatterns) {
    if (IsMatched && MatchType == Inclusive) {
      continue;
    }
    if (!IsMatched && MatchType == Exclusive) {
      continue;
    }
    auto NormalPath = pathNormalize(Path.str());

    if (Pattern.match(NormalPath)) {
      IsMatched = !IsMatched;
    }
  }
  return IsMatched;
}

}; // namespace llvm