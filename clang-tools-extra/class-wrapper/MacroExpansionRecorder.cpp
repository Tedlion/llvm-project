//===- MacroExpansionRecorder.cpp - Macro expansion information -*- C++ -*-===//
//
//  Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
//  See https://llvm.org/LICENSE.txt for license information.
//  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ===----------------------------------------------------------------------===/


#include "MacroExpansionRecorder.h"
#include "llvm/Support/Debug.h"
#include <optional>
#include <algorithm>

#define DEBUG_TYPE "macro-expansion-recorder"
#define LLVM_DEBUG(x) x

static void dumpTokenInto(const clang::Preprocessor &PP, llvm::raw_ostream &OS,
                          clang::Token Tok);

namespace clang {
namespace class_wrapper {
namespace detail {
class MacroExpansionRecorderCallback : public PPCallbacks {
  const Preprocessor &PP;
  SourceManager &SM;
  MacroExpansionRecorder::ExpansionRangeMap &ExpansionRanges;

public:
  explicit MacroExpansionRecorderCallback(
      const Preprocessor &PP, SourceManager &SM,
      MacroExpansionRecorder::ExpansionRangeMap &ExpansionRanges)
      : PP(PP), SM(SM), ExpansionRanges(ExpansionRanges) {}

  void MacroExpands(const Token &MacroName, const MacroDefinition &MD,
                    SourceRange Range, const MacroArgs *Args) override {
    // Ignore annotation tokens like: _Pragma("pack(push, 1)")
    if (MacroName.getIdentifierInfo()->getName() == "_Pragma")
      return;

    SourceLocation MacroNameBegin = SM.getExpansionLoc(MacroName.getLocation());
    assert(MacroNameBegin == SM.getExpansionLoc(Range.getBegin()));

    const SourceLocation ExpansionEnd = [Range, &SM = SM, &MacroName] {
      // If the range is empty, use the length of the macro.
      if (Range.getBegin() == Range.getEnd())
        return SM.getExpansionLoc(
            MacroName.getLocation().getLocWithOffset(MacroName.getLength()));

      // Include the last character.
      return SM.getExpansionLoc(Range.getEnd()).getLocWithOffset(1);
    }();

    (void)PP;
    LLVM_DEBUG(llvm::dbgs() << "MacroExpands event: '";
               dumpTokenInto(PP, llvm::dbgs(), MacroName);
               llvm::dbgs()
               << "' with length " << MacroName.getLength() << " at ";
               MacroNameBegin.print(llvm::dbgs(), SM);
               llvm::dbgs() << ", expansion end at ";
               ExpansionEnd.print(llvm::dbgs(), SM); llvm::dbgs() << '\n';);

    // If the expansion range is empty, use the identifier of the macro as a
    // range.
    MacroExpansionRecorder::ExpansionRangeMap::iterator It;
    bool Inserted;
    std::tie(It, Inserted) =
        ExpansionRanges.try_emplace(MacroNameBegin, ExpansionEnd);
    if (Inserted) {
      LLVM_DEBUG(llvm::dbgs() << "maps ";
                 It->getFirst().print(llvm::dbgs(), SM); llvm::dbgs() << " to ";
                 It->getSecond().print(llvm::dbgs(), SM);
                 llvm::dbgs() << '\n';);
    } else {
      if (SM.isBeforeInTranslationUnit(It->getSecond(), ExpansionEnd)) {
        It->getSecond() = ExpansionEnd;
        LLVM_DEBUG(
            llvm::dbgs() << "remaps "; It->getFirst().print(llvm::dbgs(), SM);
            llvm::dbgs() << " to "; It->getSecond().print(llvm::dbgs(), SM);
            llvm::dbgs() << '\n';);
      }
    }
  }
};

}
}
}

using namespace clang;
using namespace clang::class_wrapper;

MacroExpansionRecorder::MacroExpansionRecorder(const LangOptions &LangOpts)
    : LangOpts(LangOpts) {}

void MacroExpansionRecorder::registerForPreprocessor(Preprocessor &NewPP) {
  PP = &NewPP;
  SM = &NewPP.getSourceManager();

  // Make sure that the Preprocessor does not outlive the MacroExpansionRecorder.
  PP->addPPCallbacks(std::make_unique<detail::MacroExpansionRecorderCallback>(
      *PP, *SM, ExpansionRanges));
  // Same applies here.
  PP->setTokenWatcher([this](const Token &Tok) { onTokenLexed(Tok); });
}

std::optional<StringRef>
MacroExpansionRecorder::getExpandedText(SourceLocation MacroExpansionLoc) const {
  if (MacroExpansionLoc.isMacroID())
    return std::nullopt;

  // If there was no macro expansion at that location, return std::nullopt.
  if (ExpansionRanges.find_as(MacroExpansionLoc) == ExpansionRanges.end())
    return std::nullopt;

  // There was macro expansion, but resulted in no tokens, return empty string.
  const auto It = ExpandedTokens.find_as(MacroExpansionLoc);
  if (It == ExpandedTokens.end())
    return StringRef{""};

  // Otherwise we have the actual token sequence as string.
  return It->getSecond().first.str();
}

std::optional<StringRef>
MacroExpansionRecorder::getOriginalText(SourceLocation MacroExpansionLoc) const {
  if (MacroExpansionLoc.isMacroID())
    return std::nullopt;

  const auto It = ExpansionRanges.find_as(MacroExpansionLoc);
  if (It == ExpansionRanges.end())
    return std::nullopt;

  assert(It->getFirst() != It->getSecond() &&
         "Every macro expansion must cover a non-empty range.");

  return Lexer::getSourceText(
      CharSourceRange::getCharRange(It->getFirst(), It->getSecond()), *SM,
      LangOpts);
}

void MacroExpansionRecorder::dumpExpansionRanges() const {
  dumpExpansionRangesToStream(llvm::dbgs());
}
void MacroExpansionRecorder::dumpExpandedTexts() const {
  dumpExpandedTextsToStream(llvm::dbgs());
}

void MacroExpansionRecorder::dumpExpansionRangesToStream(raw_ostream &OS) const {
  std::vector<std::pair<SourceLocation, SourceLocation>> LocalExpansionRanges;
  LocalExpansionRanges.reserve(ExpansionRanges.size());
  for (const auto &Record : ExpansionRanges)
    LocalExpansionRanges.emplace_back(
        std::make_pair(Record.getFirst(), Record.getSecond()));
  llvm::sort(LocalExpansionRanges);

  OS << "\n=============== ExpansionRanges ===============\n";
  for (const auto &Record : LocalExpansionRanges) {
    OS << "> ";
    Record.first.print(OS, *SM);
    OS << ", ";
    Record.second.print(OS, *SM);
    OS << '\n';
  }
}

void MacroExpansionRecorder::dumpExpandedTextsToStream(raw_ostream &OS) const {
  std::vector<std::pair<SourceLocation, MacroExpansionText>>
      LocalExpandedTokens;
  LocalExpandedTokens.reserve(ExpandedTokens.size());
  for (const auto &Record : ExpandedTokens)
    LocalExpandedTokens.emplace_back(
        std::make_pair(Record.getFirst(), Record.getSecond().first));
  llvm::sort(LocalExpandedTokens);

  OS << "\n=============== ExpandedTokens ===============\n";
  for (const auto &Record : LocalExpandedTokens) {
    OS << "> ";
    Record.first.print(OS, *SM);
    OS << " -> '" << Record.second << "'\n";
  }
}

static void dumpTokenInto(const Preprocessor &PP, raw_ostream &OS, Token Tok) {
  assert(Tok.isNot(tok::raw_identifier));

  // Ignore annotation tokens like: _Pragma("pack(push, 1)")
  if (Tok.isAnnotation())
    return;

  if (IdentifierInfo *II = Tok.getIdentifierInfo()) {
    // FIXME: For now, we don't respect whitespaces between macro expanded
    // tokens. We just emit a space after every identifier to produce a valid
    // code for `int a ;` like expansions.
    //              ^-^-- Space after the 'int' and 'a' identifiers.
    OS << II->getName() << ' ';
  } else if (Tok.isLiteral() && !Tok.needsCleaning() && Tok.getLiteralData()) {
    OS << StringRef(Tok.getLiteralData(), Tok.getLength());
  } else {
    char Tmp[256];
    if (Tok.getLength() < sizeof(Tmp)) {
      const char *TokPtr = Tmp;
      // FIXME: Might use a different overload for cleaner callsite.
      unsigned Len = PP.getSpelling(Tok, TokPtr);
      OS.write(TokPtr, Len);
    } else {
      OS << "<too long token>";
    }
  }
}

void MacroExpansionRecorder::onTokenLexed(const Token &Tok) {
  SourceLocation SLoc = Tok.getLocation();
  if (SLoc.isFileID())
    return;

  // SourceLocation SpellingLoc = SM->getSpellingLoc(SLoc);

  LLVM_DEBUG(llvm::dbgs() << "lexed macro expansion token '";
             dumpTokenInto(*PP, llvm::dbgs(), Tok); llvm::dbgs() << "' at ";
             SLoc.print(llvm::dbgs(), *SM);
             llvm::dbgs() << " " << SLoc.getRawEncoding() << '\n';);

  // Remove spelling location.
  SourceLocation CurrExpansionLoc = SM->getExpansionLoc(SLoc);
  llvm::dbgs() << "CurrExpansionLoc: " << CurrExpansionLoc.printToString(*SM)
               << " " << CurrExpansionLoc.getRawEncoding() << '\n';

  auto trackDumpTokenInfo = [&PP = PP, &Tok, SLoc](
      MacroExpansionText &TokenAsString, TokenList &Tokens) {
    llvm::raw_svector_ostream OS(TokenAsString);
    unsigned LengthBefore = TokenAsString.size();
    dumpTokenInto(*PP, OS, Tok);
    unsigned LengthAfter = TokenAsString.size();
    if (LengthAfter == LengthBefore)
      return;
    Tokens.emplace_back(SLoc, LengthAfter, TokenAsString[LengthBefore] == ' ');
  };

  if (auto it = ExpandedTokens.find(CurrExpansionLoc);
    it != ExpandedTokens.end()) {
    auto &[TokenAsString, Tokens] = it->getSecond();
    trackDumpTokenInfo(TokenAsString, Tokens);
  } else {
    MacroExpansionText TokenAsString;
    TokenList Tokens;
    trackDumpTokenInfo(TokenAsString, Tokens);
    ExpandedTokens.try_emplace(CurrExpansionLoc,
      std::make_pair(std::move(TokenAsString), std::move(Tokens)));
  }
}


std::optional<MacroExpansionRecorder::ExpansionTokens>
MacroExpansionRecorder::getExpansionTokensWithin(SourceLocation MacroExpansionLoc,
                      SourceLocation Begin, SourceLocation End) const {
  if (MacroExpansionLoc.isMacroID())
    return std::nullopt;

  // If there was no macro expansion at that location, return std::nullopt.
  if (ExpansionRanges.find_as(MacroExpansionLoc) == ExpansionRanges.end())
    return std::nullopt;

  // There was macro expansion, but resulted in no tokens, return empty string.
  const auto It = ExpandedTokens.find_as(MacroExpansionLoc);
  if (It == ExpandedTokens.end())
    return std::nullopt;

  const auto & [TokenAsString, Tokens] = It->getSecond();
  auto BeginIt = Begin.isInvalid()
                   ? std::ranges::find(Tokens, Begin, &TokenSpellingLoc::Loc)
                   : Tokens.begin();

  auto EndIt = End.isInvalid()
                 ? std::ranges::find(Tokens, End, &TokenSpellingLoc::Loc)
                 : Tokens.end();

  if (BeginIt == Tokens.end() || EndIt == Tokens.end() || BeginIt > EndIt) {
    return std::nullopt;
  }

  // Token's SourceLocation is not guaranteed to be globally unique,
  // return std::nullopt if the Beginning or End location is not unique.
  if (Begin.isInvalid()) {
    if (auto SecondMatch = std::ranges::find(
          std::next(BeginIt), Tokens.end(), Begin, &TokenSpellingLoc::Loc);
      SecondMatch != Tokens.end()) {
      return std::nullopt;
    }
  }
  if (End.isInvalid()) {
    if (auto SecondMatch = std::ranges::find(
          std::next(EndIt), Tokens.end(), End, &TokenSpellingLoc::Loc);
      SecondMatch != Tokens.end()) {
      return std::nullopt;
    }
  }

  unsigned BeginOffset = BeginIt == Tokens.begin()
                             ? 0
                             : std::prev(BeginIt)->EndingOffset;

  return ExpansionTokens(TokenAsString, ArrayRef(BeginIt, std::next(EndIt)),
                         BeginOffset);
}