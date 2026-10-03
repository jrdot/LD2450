/**
 * @file ansi_color.hpp
 * @brief ANSI 터미널 색상 및 텍스트 스타일 유틸리티
 *
 * @details
 * ANSI Escape Sequence를 사용하여 터미널의 글자색, 배경색,
 * 텍스트 스타일을 제어합니다.
 *
 * 지원 기능:
 * - 기본 8색 / 밝은 8색
 * - 256 Color
 * - RGB True Color
 * - Bold, Dim, Underline, Inverse 등 텍스트 스타일
 *
 * @code
 * #include "ansi_color.hpp"
 *
 * std::cout << cli::fg::RED
 *           << "ERROR"
 *           << cli::style::RESET
 *           << '\n';
 *
 * std::cout << cli::fgRGB(255, 128, 0)
 *           << "Orange"
 *           << cli::style::RESET
 *           << '\n';
 * @endcode
 *
 * @note ANSI Escape Sequence를 지원하는 터미널에서 사용해야 합니다.
 */

#pragma once

#include <string>
#include <string_view>


namespace cli {

// ============================================================================
// Text Style
// ============================================================================

/**
 * @brief ANSI 텍스트 스타일
 *
 * @code
 * std::cout << cli::style::BOLD
 *           << "Bold Text"
 *           << cli::style::RESET;
 * @endcode
 */
namespace style {

inline constexpr std::string_view RESET     = "\033[0m";
inline constexpr std::string_view BOLD      = "\033[1m";
inline constexpr std::string_view DIM       = "\033[2m";
inline constexpr std::string_view UNDERLINE = "\033[4m";
inline constexpr std::string_view INVERSE   = "\033[7m";
inline constexpr std::string_view HIDDEN    = "\033[8m";

} // namespace style


// ============================================================================
// Foreground Color
// ============================================================================

/**
 * @brief ANSI 글자색(Foreground)
 *
 * @code
 * std::cout << cli::fg::RED
 *           << "Red Text"
 *           << cli::style::RESET;
 * @endcode
 */
namespace fg {

// 기본 색상
inline constexpr std::string_view BLACK   = "\033[30m";
inline constexpr std::string_view RED     = "\033[31m";
inline constexpr std::string_view GREEN   = "\033[32m";
inline constexpr std::string_view YELLOW  = "\033[33m";
inline constexpr std::string_view BLUE    = "\033[34m";
inline constexpr std::string_view MAGENTA = "\033[35m";
inline constexpr std::string_view CYAN    = "\033[36m";
inline constexpr std::string_view WHITE   = "\033[37m";


// 밝은 색상
inline constexpr std::string_view BRIGHT_BLACK   = "\033[90m";
inline constexpr std::string_view BRIGHT_RED     = "\033[91m";
inline constexpr std::string_view BRIGHT_GREEN   = "\033[92m";
inline constexpr std::string_view BRIGHT_YELLOW  = "\033[93m";
inline constexpr std::string_view BRIGHT_BLUE    = "\033[94m";
inline constexpr std::string_view BRIGHT_MAGENTA = "\033[95m";
inline constexpr std::string_view BRIGHT_CYAN    = "\033[96m";
inline constexpr std::string_view BRIGHT_WHITE   = "\033[97m";

} // namespace fg


// ============================================================================
// Background Color
// ============================================================================

/**
 * @brief ANSI 배경색(Background)
 *
 * @code
 * std::cout << cli::bg::BLUE
 *           << cli::fg::WHITE
 *           << "White on Blue"
 *           << cli::style::RESET;
 * @endcode
 */
namespace bg {

// 기본 색상
inline constexpr std::string_view BLACK   = "\033[40m";
inline constexpr std::string_view RED     = "\033[41m";
inline constexpr std::string_view GREEN   = "\033[42m";
inline constexpr std::string_view YELLOW  = "\033[43m";
inline constexpr std::string_view BLUE    = "\033[44m";
inline constexpr std::string_view MAGENTA = "\033[45m";
inline constexpr std::string_view CYAN    = "\033[46m";
inline constexpr std::string_view WHITE   = "\033[47m";


// 밝은 색상
inline constexpr std::string_view BRIGHT_BLACK   = "\033[100m";
inline constexpr std::string_view BRIGHT_RED     = "\033[101m";
inline constexpr std::string_view BRIGHT_GREEN   = "\033[102m";
inline constexpr std::string_view BRIGHT_YELLOW  = "\033[103m";
inline constexpr std::string_view BRIGHT_BLUE    = "\033[104m";
inline constexpr std::string_view BRIGHT_MAGENTA = "\033[105m";
inline constexpr std::string_view BRIGHT_CYAN    = "\033[106m";
inline constexpr std::string_view BRIGHT_WHITE   = "\033[107m";

} // namespace bg


// ============================================================================
// 256 Color
// ============================================================================

/**
 * @brief 256 Color 글자색 코드를 생성합니다.
 *
 * @param index 색상 번호 (0 ~ 255)
 * @return ANSI 글자색 Escape Sequence
 *
 * @code
 * std::cout << cli::fg256(202)
 *           << "256 Color"
 *           << cli::style::RESET;
 * @endcode
 */
inline std::string fg256(int index)
{
    return "\033[38;5;" +
           std::to_string(index) +
           "m";
}


/**
 * @brief 256 Color 배경색 코드를 생성합니다.
 *
 * @param index 색상 번호 (0 ~ 255)
 * @return ANSI 배경색 Escape Sequence
 *
 * @code
 * std::cout << cli::bg256(24)
 *           << "256 Color Background"
 *           << cli::style::RESET;
 * @endcode
 */
inline std::string bg256(int index)
{
    return "\033[48;5;" +
           std::to_string(index) +
           "m";
}


// ============================================================================
// RGB True Color
// ============================================================================

/**
 * @brief RGB True Color 글자색 코드를 생성합니다.
 *
 * @param r Red   (0 ~ 255)
 * @param g Green (0 ~ 255)
 * @param b Blue  (0 ~ 255)
 *
 * @return ANSI RGB 글자색 Escape Sequence
 *
 * @code
 * std::cout << cli::fgRGB(255, 128, 0)
 *           << "Orange"
 *           << cli::style::RESET;
 * @endcode
 */
inline std::string fgRGB(int r, int g, int b)
{
    return "\033[38;2;" +
           std::to_string(r) + ";" +
           std::to_string(g) + ";" +
           std::to_string(b) +
           "m";
}


/**
 * @brief RGB True Color 배경색 코드를 생성합니다.
 *
 * @param r Red   (0 ~ 255)
 * @param g Green (0 ~ 255)
 * @param b Blue  (0 ~ 255)
 *
 * @return ANSI RGB 배경색 Escape Sequence
 *
 * @code
 * std::cout << cli::bgRGB(20, 40, 80)
 *           << cli::fg::WHITE
 *           << "RGB Background"
 *           << cli::style::RESET;
 * @endcode
 */
inline std::string bgRGB(int r, int g, int b)
{
    return "\033[48;2;" +
           std::to_string(r) + ";" +
           std::to_string(g) + ";" +
           std::to_string(b) +
           "m";
}

} // namespace cli