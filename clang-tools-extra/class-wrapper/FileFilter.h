/**
 * @brief
 * @authors tangwy
 * @date 2024/4/7
 */

#ifndef FILEFILTER_H
#define FILEFILTER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/GlobPattern.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace llvm {
extern std::string pathNormalize(const std::string &Path);

class OwnedGlobPattern {
public:
  OwnedGlobPattern(OwnedGlobPattern &&) = default;
  OwnedGlobPattern &operator=(OwnedGlobPattern &&) = default;
  OwnedGlobPattern(const OwnedGlobPattern &) = delete;
  OwnedGlobPattern &operator=(const OwnedGlobPattern &) = delete;

  bool match(StringRef Path) const { return Pattern.match(Path); }

  static Expected<OwnedGlobPattern> create(StringRef PatternText);

private:
  std::unique_ptr<char[]> PatternStorage;
  GlobPattern Pattern;


  OwnedGlobPattern(std::unique_ptr<char[]> PatternStorage, GlobPattern Pattern)
    : PatternStorage(std::move(PatternStorage)),
      Pattern(std::move(Pattern)) {
  }
};

class FileFilter {
public:
  FileFilter(std::vector<std::string>::const_iterator RuleBegin,
             std::vector<std::string>::const_iterator RuleEnd,
             StringRef SrcRoot);

  bool isMatched(StringRef Path) const;

private:
  enum MatchType{
    Inclusive,
    Exclusive,
  };

  std::vector<std::pair<MatchType, OwnedGlobPattern> > FilePathPatterns;
};

} // namespace llvm

#endif // FILEFILTER_H
