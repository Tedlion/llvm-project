/**
 * @brief
 * @authors tangwy
 * @date 2024/4/7
 */

#ifndef FILEFILTER_H
#define FILEFILTER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/GlobPattern.h"

#include <utility>
#include <vector>

namespace llvm {

class FileFilter {
public:
  FileFilter(std::vector<std::string>::const_iterator FilesBegin,
             std::vector<std::string>::const_iterator FilesEnd,
             StringRef SrcRoot);

  bool isMatched(StringRef Path) const;

private:
  enum MatchType{
    Inclusive,
    Exclusive,
  };

  // FIXME: FilePathPatterns has a StringRef member and brings lifetime issue!
  std::vector<std::pair<MatchType, GlobPattern>> FilePathPatterns;

};

} // namespace llvm

#endif // FILEFILTER_H
