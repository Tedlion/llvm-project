//===- MacroExpansionRecorder.h - Macro expansion information ---*- C++ -*-===//
//
//  Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
//  See https://llvm.org/LICENSE.txt for license information.
//  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------------===/
//
//  This file defines the class MacroExpansionRecorder, which is extensive
//  version of MacroExpansionContext. Other than the basic function of recording
//  the "ExpandedText" and "OriginalText", it also records the SourceLocation of
//  all tokens in the expansion. So we can find the partial expansion within a
//  given SourceRange. It may be used to retrieve the specific tokens or text of
//  a matched AST node.
//
//===-----------------------------------------------------------------------===/

#ifndef MACROEXPANSIONRECORDER_H
#define MACROEXPANSIONRECORDER_H

#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace clang {
namespace class_wrapper {

namespace detail {
class MacroExpansionRecorderCallback;
} // namespace detail

class MacroExpansionRecorder {
public:
  /// Creates a MacroExpansionRecorder.
  /// \remark You must call registerForPreprocessor to set the required
  ///         onTokenLexed callback and the PPCallbacks.
  explicit MacroExpansionRecorder(const LangOptions &LangOpts);

  /// Register the necessary callbacks to the Preprocessor to record the
  /// expansion events and the generated tokens. Must ensure that this object
  /// outlives the given Preprocessor.
  void registerForPreprocessor(Preprocessor &PP);

  /// \param MacroExpansionLoc Must be the expansion location of a macro.
  /// \return The textual representation of the token sequence which was
  ///         substituted in place of the macro after the preprocessing.
  ///         If no macro was expanded at that location, returns std::nullopt.
  std::optional<StringRef>
  getExpandedText(SourceLocation MacroExpansionLoc) const;

  /// \param MacroExpansionLoc Must be the expansion location of a macro.
  /// \return The text from the original source code which were substituted by
  ///         the macro expansion chain from the given location.
  ///         If no macro was expanded at that location, returns std::nullopt.
  std::optional<StringRef>
  getOriginalText(SourceLocation MacroExpansionLoc) const;

  struct TokenSpellingLoc {
    SourceLocation Loc;

    unsigned EndingOffset   : 31;
    unsigned IsSpaceBefore  : 1;
  };

  class ExpansionTokens {
    friend class MacroExpansionRecorder;
    StringRef ExpansionText; // the full text of the macro expansion
    ArrayRef<TokenSpellingLoc> Tokens;
    unsigned BeginOffset;

  public:
    class iterator {
      friend class ExpansionTokens;
      const TokenSpellingLoc *p;
      unsigned BeginOffset;
      StringRef ExpansionText;


      iterator(const TokenSpellingLoc *p, unsigned BeginOffset,
               StringRef ExpansionText) : p(p),
                                          BeginOffset(BeginOffset),
                                          ExpansionText(ExpansionText) {}

    public:
      using value_type = std::pair<SourceLocation, StringRef>;


      value_type operator*() const {
        unsigned TokBegin = BeginOffset + (p->IsSpaceBefore ? 0 : 1);
        unsigned Length = p->EndingOffset - TokBegin;
        return {p->Loc, StringRef{ExpansionText.data() + TokBegin, Length}};
      }


      iterator &operator++() {
        BeginOffset = p->EndingOffset;
        ++p;
        return *this;
      }


      bool operator==(const iterator &other) const {
        return p == other.p;
      }
    };


    iterator begin() {
      return iterator(Tokens.begin(), BeginOffset, ExpansionText);
    }


    iterator end() {
      return iterator(Tokens.end(), 0, ExpansionText);
    }


    StringRef getText() const {
      unsigned EndOffset = Tokens.back().EndingOffset;
      return StringRef{ExpansionText.data() + BeginOffset,
                       EndOffset - BeginOffset};
    }


    std::optional<std::pair<unsigned/*Offset*/, unsigned/*Length*/> >
    getSpellingPosFor(SourceLocation Loc) const {
      for (const auto &Tok : Tokens) {
        if (Tok.Loc == Loc) {
          unsigned TokBegin = BeginOffset + (Tok.IsSpaceBefore ? 0 : 1);
          unsigned Length = Tok.EndingOffset - TokBegin;
          return std::make_pair(TokBegin, Length);
        }
      }
      return std::nullopt;
    }
  };

  /// \param MacroExpansionLoc Must be the expansion location of a macro.
  /// \param Begin The beginning token location in the expansion.
  ///        If it is invalid, then starts from the beginning of the expansion.
  /// \param End The end token location in the expansion. If it is invalid,
  ///        then ends at the end of the expansion.
  /// \return The partial expanded text within the given range.
  ///         If no macro was expanded at that location, or Begin or End
  ///         location not found in the expansion, returns std::nullopt.
  std::optional<ExpansionTokens>
  getExpansionTokensWithin(SourceLocation MacroExpansionLoc,
                        SourceLocation Begin, SourceLocation End) const;

  void dumpExpansionRangesToStream(raw_ostream &OS) const;
  void dumpExpandedTextsToStream(raw_ostream &OS) const;
  void dumpExpansionRanges() const;
  void dumpExpandedTexts() const;

private:
  friend class detail::MacroExpansionRecorderCallback;
  using MacroExpansionText = SmallString<40>;
  using ExpansionMap = llvm::DenseMap<SourceLocation, MacroExpansionText>;
  using ExpansionRangeMap = llvm::DenseMap<SourceLocation, SourceLocation>;

  /// Associates the textual representation of the expanded tokens at the given
  /// macro expansion location.
  ExpansionMap ExpandedTokens;

  /// Tracks which source location was the last affected by any macro
  /// substitution starting from a given macro expansion location.
  ExpansionRangeMap ExpansionRanges;

  Preprocessor *PP = nullptr;
  SourceManager *SM = nullptr;
  const LangOptions &LangOpts;

  /// This callback is called by the preprocessor.
  /// It stores the textual representation of the expanded token sequence for a
  /// macro expansion location.
  void onTokenLexed(const Token &Tok);
};
}
}


#endif //MACROEXPANSIONRECORDER_H
