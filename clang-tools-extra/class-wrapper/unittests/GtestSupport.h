/**
 * @brief
 * @authors tangwy
 * @date 2024/5/11
 */

#ifndef GTESTSUPPORT_H
#define GTESTSUPPORT_H

#include "gtest/gtest.h"
#include <expected>
#include <format>

template <typename T>
concept Formattable = requires(T) { std::formatter<T>{}; };

template <typename T>
std::string formatExpectedValue(const std::string &Name, const T &) {
  return std::format("\"{}\" expected value, but got error", Name);
}

template <Formattable T>
std::string formatExpectedValue(const std::string &Name, const T &Error) {
  return std::format("\"{}\" expected value, but got error: {}", Name, Error);
}

#define ASSERT_EXPECTED_VALUE(expected)                                        \
  if (!(expected).has_value()) {                                               \
    GTEST_FATAL_FAILURE_(                                                      \
        formatExpectedValue(#expected, (expected).error()).data());            \
  }

#define EXPECT_EXPECTED_VALUE(expected)                                        \
  if (!(expected).has_value()) {                                               \
    GTEST_NONFATAL_FAILURE_(                                                      \
        formatExpectedValue(#expected, (expected).error()).data());            \
  }


template <typename T>
std::string formatExpectedError(const std::string &Name, const T &) {
  return std::format("\"{}\"expected error, but got value", Name);
}

template <Formattable T>
std::string formatExpectedError(const std::string &Name, const T &Value) {
  return std::format("\"{}\" expected error, but got value: {}", Name, Value);
}

#define ASSERT_EXPECTED_ERROR(expected)                                        \
  if ((expected).has_value()) {                                                \
    GTEST_FATAL_FAILURE_(                                                      \
        formatExpectedError(#expected, (expected).value()).data());            \
  }

#define EXPECT_EXPECTED_ERROR(expected)                                        \
  if ((expected).has_value()) {                                                \
    GTEST_NONFATAL_FAILURE_(                                                   \
        formatExpectedError(#expected, (expected).value()).data());            \
  }
#endif // GTESTSUPPORT_H
