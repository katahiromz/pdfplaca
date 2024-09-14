// pdfplaca.cpp --- Make a PDF placard by katahiromz
// License: Apache 2.0
#define _CRT_SECURE_NO_DEPRECATE    // We use deprecate functions for compatibility
#define _USE_MATH_DEFINES           // We use <cmath> M_PI constant

#include <cstdlib>          // C Standard Library
#include <cstdio>           // C Standard Input/Output Library
#include <cstdint>          // C Standard Integers
#include <cmath>            // C Math Library
#include <cassert>          // For assert macro
#include <vector>           // For std::vector
#include <string>           // For std::string and std::wstring
#include <complex>          // For std::complex
#include <algorithm>        // For standard algorithm

// For detecting memory leak (for MSVC only)
#if defined(_MSC_VER) && !defined(NDEBUG) && !defined(_CRTDBG_MAP_ALLOC)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>     // For C run-time debugging
#endif

#include <cairo.h>          // Cairo Graphic Library
#include <cairo-pdf.h>      // Cairo PDF

#include <windows.h>        // Windows standard header
#include <windowsx.h>       // Windows helper macros
#include <shlwapi.h>        // Shell Light-weight API
#include <tchar.h>          // Generic text mapping
#include <strsafe.h>        // Safe string manipulation

#include "color_value.h"    // Color values
#include "page_size.h"      // Page sizes

// Show version info
void pdfplaca_version(void)
{
    std::printf("pdfplaca by katahiromz Version 0.85\n");
}

// Get the default font
const _TCHAR *pdfplaca_get_default_font(void)
{
    if (PRIMARYLANGID(GetUserDefaultLangID()) == LANG_JAPANESE) // Is the user Japanese?
        return _T("MS Gothic");
    return _T("Tahoma");
}

// Show help message
void pdfplaca_usage(void)
{
    std::printf(
        "Usage: pdfplaca [OPTIONS]\n"
        "Options:\n"
        "  --text \"TEXT\"             Specify output text (default: \"This is\\na test.\")\n"
        "  -o output.pdf             Specify output PDF filename (default: output.pdf)\n"
        "  --page-size WIDTHxHEIGHT  Specify page size in mm (default: A4).\n"
        "  --landscape               Use landscape orientation.\n"
        "  --portrait                Use portrait orientation.\n"
        "  --font \"FONT\"             Specify font name (default: \"%ls\").\n"
        "  --margin MARGIN           Specify page margin in mm (default: 8).\n"
        "  --text-color #RRGGBB      Specify text color (default: black).\n"
        "  --back-color #RRGGBB      Specify background color (default: white).\n"
        "  --threshold THRESHOLD     Specify aspect ratio threshold (default: 1.5).\n"
        "  --letters-per-page NUM    Specify letters per page (default: -1)\n"
        "  --vertical                Use vertical writing.\n"
        "  --y-adjust VALUE          Y adjustment in mm (default: 0).\n"
        "  --font-list               List font entries.\n"
        "  --help                    Display this message.\n"
        "  --version                 Display version information.\n",
        pdfplaca_get_default_font()
    );
}

// Global variables
const _TCHAR *g_out_text = _T("This is\na test.");
const _TCHAR *g_out_file = _T("output.pdf");
const _TCHAR *g_font_name = pdfplaca_get_default_font();
double g_page_width = -1;
double g_page_height = -1;
double g_margin = 8;
bool g_usage = false;
bool g_version = false;
bool g_font_list = false;
bool g_vertical = false;
const _TCHAR *g_orientation = _T("landscape");
uint32_t g_text_color = 0x000000;
uint32_t g_back_color = 0xFFFFFF;
double g_threshold = 1.5;
double g_y_adjust = 0;
int g_letters_per_page = -1;
bool g_fixed_pitch_font = false;

// 単位をmmからptへ変換する。
constexpr double pt_from_mm(double mm)
{
    return mm * (72.0 / 25.4);
}

// ワイド文字列からANSI文字列に変換する。
std::string ansi_from_wide(const wchar_t *wide, int codepage = CP_UTF8)
{
    static char s_buf[1024];
    s_buf[0] = 0;
    WideCharToMultiByte(codepage, 0, wide, -1, s_buf, _countof(s_buf), nullptr, nullptr);
    s_buf[_countof(s_buf) - 1] = 0;
    return s_buf;
}

// ANSI文字列からワイド文字列に変換する。
std::wstring wide_from_ansi(const char *ansi, int codepage = CP_UTF8)
{
    static wchar_t s_buf[1024];
    s_buf[0] = 0;
    MultiByteToWideChar(codepage, 0, ansi, -1, s_buf, _countof(s_buf));
    s_buf[_countof(s_buf) - 1] = 0;
    return s_buf;
}

// RGBから赤の値を取得する。
constexpr uint8_t get_r_value(uint32_t color)
{
    return (color >> 16) & 0xFF;
}
// RGBから緑の値を取得する。
constexpr uint8_t get_g_value(uint32_t color)
{
    return (color >> 8) & 0xFF;
}
// RGBから青の値を取得する。
constexpr uint8_t get_b_value(uint32_t color)
{
    return (color >> 0) & 0xFF;
}

// UTF-8シーケンスの最初のバイトかどうか判定する。
constexpr bool u8_is_lead(unsigned char ch)
{
    return (ch & 0xC0) != 0x80;
}

// UTF-8文字列の実際の文字数を取得する。
constexpr size_t u8_len(const char *str)
{
    size_t len = 0;
    while (*str)
    {
        if (u8_is_lead(*str))
            ++len;
        ++str;
    }
    return len;
}
static_assert(u8_len(u8"abあいう漢字") == 7, "");
static_assert(u8_len(u8"𠮷") == 1, "");
static_assert(u8_len(u8"😃😃") == 2, "");

// UTF-8文字列の実際の文字に分割する。
void u8_split_chars(std::vector<std::string>& chars, const char *str)
{
    std::string s;
    for (const char *pch = str; *pch; ++pch)
    {
        if (u8_is_lead(*pch) && s.size())
        {
            chars.push_back(s);
            s.clear();
        }
        s += *pch;
    }
    if (s.size())
        chars.push_back(s);
}

// 文字列を置き換える。
template <typename T_STR>
constexpr bool
mstr_replace_all(T_STR& str, const T_STR& from, const T_STR& to)
{
    bool ret = false;
    size_t i = 0;
    for (;;) {
        i = str.find(from, i);
        if (i == T_STR::npos)
            break;
        ret = true;
        str.replace(i, from.size(), to);
        i += to.size();
    }
    return ret;
}
template <typename T_STR>
constexpr bool
mstr_replace_all(T_STR& str,
                 const typename T_STR::value_type *from,
                 const typename T_STR::value_type *to)
{
    return mstr_replace_all(str, T_STR(from), T_STR(to));
}

// 文字列を分割する。charsは区切りの文字集合。
template <typename T_STR_CONTAINER>
constexpr void
mstr_split(T_STR_CONTAINER& container,
           const typename T_STR_CONTAINER::value_type& str,
           const typename T_STR_CONTAINER::value_type& chars)
{
    container.clear();
    size_t i = 0, k = str.find_first_of(chars);
    while (k != T_STR_CONTAINER::value_type::npos)
    {
        container.push_back(str.substr(i, k - i));
        i = k + 1;
        k = str.find_first_of(chars, i);
    }
    container.push_back(str.substr(i));
}

// 文字列を結合する。sepは区切り。
template <typename T_STR_CONTAINER>
constexpr typename T_STR_CONTAINER::value_type
mstr_join(const T_STR_CONTAINER& container,
          const typename T_STR_CONTAINER::value_type& sep)
{
    typename T_STR_CONTAINER::value_type result;
    typename T_STR_CONTAINER::const_iterator it, end;
    it = container.begin();
    end = container.end();
    if (it != end)
    {
        result = *it;
        for (++it; it != end; ++it)
        {
            result += sep;
            result += *it;
        }
    }
    return result;
}

// 文字列をエスケープする。
std::string mstr_escape(const std::string& text)
{
    std::string ret;
    for (size_t ich = 0; ich < text.size(); ++ich)
    {
        auto ch = text[ich];
        switch (ch)
        {
        case '\t': ret += '\\'; ret += 't'; break;
        case '\n': ret += '\\'; ret += 'n'; break;
        case '\r': ret += '\\'; ret += 'r'; break;
        case '\f': ret += '\\'; ret += 'f'; break;
        case '\\': ret += '\\'; ret += '\\'; break;
        default: ret += ch; break;
        }
    }
    return ret;
}

// 文字列のエスケープを解除する。
std::string mstr_unescape(const std::string& text)
{
    std::string ret;
    bool escaping = false;
    for (size_t ich = 0; ich < text.size(); ++ich)
    {
        auto ch = text[ich];
        if (escaping)
        {
            switch (ch)
            {
            case 't': ret += '\t'; break;
            case 'n': ret += '\n'; break;
            case 'r': ret += '\r'; break;
            case 'f': ret += '\f'; break;
            case '\\': ret += '\\'; break;
            default: ret += ch; break;
            }
            escaping = false;
        }
        else
        {
            if (ch == '\\')
            {
                escaping = true;
                if (ich + 1 == text.size())
                {
                    ret += ch;
                    break;
                }
            }
            else
            {
                ret += ch;
            }
        }
    }
    return ret;
}

// UTF-8文字列を改行で分割する。
void u8_split_by_newlines(std::vector<std::string>& rows, const char *str)
{
    std::string s = str;
    mstr_replace_all(s, "\r\n", "\n");
    mstr_replace_all(s, "\r", "\n");
    mstr_split(rows, s, "\n");
}

bool u8_contains_one_of(const char *ptr, const char *char_set)
{
    std::vector<std::string> chars;
    u8_split_chars(chars, char_set);
    for (auto ch : chars)
    {
        if (ch == ptr)
            return true;
    }
    return false;
}

// 文字はスペースか？
static inline bool u8_is_space(const char *ptr)
{
    return u8_contains_one_of(ptr, u8" 　");
}

// カッコ（タイプ1）か？
static inline bool u8_is_paren_type_1(const char *ptr)
{
    return u8_contains_one_of(ptr, u8"(（[［〔【｛〈《≪｟⁅〖〘«»〙〗⁆｠≫》〉｝】〕］]）)");
}

// カッコ（タイプ2）か？
static inline bool u8_is_paren_type_2(const char *ptr)
{
    return u8_contains_one_of(ptr, u8"「『");
}

// カッコ（タイプ3）か？
static inline bool u8_is_paren_type_3(const char *ptr)
{
    return u8_contains_one_of(ptr, u8"』」");
}

// 句読点か？
static inline bool u8_is_comma_period(const char *ptr)
{
    return u8_contains_one_of(ptr, u8"、。，．");
}

// 横棒か？
static inline bool u8_is_hyphen_dash(const char *ptr)
{
    return u8_contains_one_of(ptr, u8"-－―ー=＝≡～");
}

// 小さいカナか？
static inline bool u8_is_small_kana(const char *ptr)
{
    return u8_contains_one_of(ptr, u8"ぁぃぅぇぉっゃゅょゎゕゖァィゥェォヵㇰヶㇱㇲッㇳㇴㇵㇶㇷㇸㇹㇺャュョㇻㇼㇽㇾㇿヮ");
}

// UTF-8シーケンスの最初のバイトでシーケンスの長さを判定する。
int u8_get_skip_chars(uint8_t ch)
{
    if (!(ch & 0x80))
        return 1;
    if ((ch & 0xE0) == 0xC0)
        return 2;
    if ((ch & 0xF0) == 0xE0)
        return 3;
    if ((ch & 0xF8) == 0xF0)
        return 4;
    if ((ch & 0xFC) == 0xF8)
        return 5;
    if ((ch & 0xFE) == 0xFC)
        return 6;
    assert(0);
    return -1;
}

// UTF-8からUTF-32に変換する。*skipはシーケンス長。
uint32_t u32_from_u8(const char *ptr, int *skip)
{
    uint32_t u32 = 0;
    const uint8_t *pch = reinterpret_cast<const uint8_t *>(ptr);
    int len = u8_get_skip_chars(*pch);
    *skip = len;

    switch (len)
    {
    case 1:
        u32 = *pch;
        break;
    case 2:
        u32 = ((pch[0] & 0x1F) << 6) | (pch[1] & 0x3F);
        break;
    case 3:
        u32 = ((pch[0] & 0x0F) << 12) | ((pch[1] & 0x3F) << 6) | (pch[2] & 0x3F);
        break;
    case 4:
        u32 = ((pch[0] & 0x07) << 18) | ((pch[1] & 0x3F) << 12) | ((pch[2] & 0x3F) << 6) | (pch[3] & 0x3F);
        break;
    default:
        *skip = -1;
        return 0;
    }

    return u32;
}

// UTF-8文字列が日本語テキストかどうか判定する。
int u8_is_japanese_text(const char *str)
{
    int skip;
    int ret = 0;
    for (const char *pch = str; *pch; )
    {
        uint32_t u32 = u32_from_u8(pch, &skip);
        if (skip == -1)
            return 0;
        if (0x3040 <= u32 && u32 <= 0x309F) // U+3040..U+309F: Hiragana
            if (ret < 2) ret = 2;
        if (0x30A0 <= u32 && u32 <= 0x30FF) // U+30A0..U+30FF: Katakana
            if (ret < 2) ret = 2;
        if (0x31F0 <= u32 && u32 <= 0x31FF) // U+31F0..U+31FF: Katakana Phonetic Extensions
            if (ret < 2) ret = 2;
        if (0xFF01 <= u32 && u32 <= 0xFF9D) // U+FF01..U+FF9D: Halfwidth and Fullwidth Forms
            if (ret < 1) ret = 1;
        if ((0x3400 <= u32 && u32 <= 0x4DB5) || (0x4E00 <= u32 && u32 <= 0x9FCB) ||
            (0xF900 <= u32 && u32 <= 0xFA6A)) // Kanji
        {
            if (ret < 1) ret = 1;
        }
        if (0x3000 <= u32 && u32 <= 0x303F) // U+3000..U+303F: CJK Symbols and Punctuation Block
            if (ret < 1) ret = 1;
        for (int i = 0; i < skip && *pch; ++i)
            ++pch;
    }
    return ret;
}

void u8_is_japanese_text_unittest(void)
{
#ifndef NDEBUG
    assert(!u8_is_japanese_text(u8""));
    assert(!u8_is_japanese_text(u8"ABCabc123"));
    assert(u8_is_japanese_text(u8"あ")); // Hiragana
    assert(u8_is_japanese_text(u8"ア")); // Katanaka
    assert(u8_is_japanese_text(u8"ｱ")); // Halfwidth Katanaka
    assert(u8_is_japanese_text(u8"漢字")); // Kanji
    assert(u8_is_japanese_text(u8"ＡＢＣ")); // Fullwidth ASCII
#endif
}

// UTF-8文字列が中国語テキストかどうか判定する。
int u8_is_chinese_text(const char *str)
{
    int skip;
    for (const char *pch = str; *pch; )
    {
        uint32_t u32 = u32_from_u8(pch, &skip);
        if (skip == -1)
            return 0;
        if (0x4E00 <= u32 && u32 <= 0x9FFF) // U+4E00..U+9FFF: CJK Unified Ideographs
            return 1;
        if (0xF900 <= u32 && u32 <= 0xFAFF) // U+F900..U+FAFF: CJK Compatibility Ideographs
            return 1;
        if (0x2F00 <= u32 && u32 <= 0x2FDF) // U+2F00..U+2FDF: CJK Radicals
            return 1;
        if (0x2E80 <= u32 && u32 <= 0x2EFF) // U+2E80..U+2EFF: CJK Radicals Supplement
            return 1;
        if (0x3400 <= u32 && u32 <= 0x4DBF) // U+3400..U+4DBF CJK Unified Ideographs Extension A
            return 1;
        if (0x20000 <= u32 && u32 <= 0x2A6DF) // U+20000..U+2A6DF CJK Unified Ideographs Extension B
            return 1;
        if (0x2A700 <= u32 && u32 <= 0x2B73F) // U+2A700..U+2B73F CJK Unified Ideographs Extension C
            return 1;
        if (0x2B740 <= u32 && u32 <= 0x2B81F) // U+2B740..U+2B81F CJK Unified Ideographs Extension D
            return 1;
        if (0x2B820 <= u32 && u32 <= 0x2CEAF) // U+2B820..U+2CEAF CJK Unified Ideographs Extension E
            return 1;
        if (0x2CEB0 <= u32 && u32 <= 0x2EBEF) // U+2CEB0..U+2EBEF CJK Unified Ideographs Extension F
            return 1;
        if (0x30000 <= u32 && u32 <= 0x3134F) // U+30000..U+3134F CJK Unified Ideographs Extension G
            return 1;
        if (0x31350 <= u32 && u32 <= 0x323AF) // U+31350..U+323AF CJK Unified Ideographs Extension H
            return 1;
        if (0x3000 <= u32 && u32 <= 0x303F) // U+3000..U+303F: CJK Symbols and Punctuation Block
            return 1;
        if (0x2F800 <= u32 && u32 <= 0x2FA1F) // U+2F800..U+2FA1F: Unifiable variants
            return 1;
        for (int i = 0; i < skip && *pch; ++i)
            ++pch;
    }
    return 0;
}

// UTF-8文字列が韓国語テキストかどうか判定する。
int u8_is_korean_text(const char *str)
{
    int skip;
    int ret = 0;
    for (const char *pch = str; *pch; )
    {
        uint32_t u32 = u32_from_u8(pch, &skip);
        if (skip == -1)
            return 0;
        if (0x4E00 <= u32 && u32 <= 0x9FFF) // U+4E00..U+9FFF: CJK Unified Ideographs
            if (ret < 1) ret = 1;
        if (0xF900 <= u32 && u32 <= 0xFAFF) // U+F900..U+FAFF: CJK Compatibility Ideographs
            if (ret < 1) ret = 1;
        if (0x2F00 <= u32 && u32 <= 0x2FDF) // U+2F00..U+2FDF: CJK Radicals
            if (ret < 1) ret = 1;
        if (0x2E80 <= u32 && u32 <= 0x2EFF) // U+2E80..U+2EFF: CJK Radicals Supplement
            if (ret < 1) ret = 1;
        if (0x3000 <= u32 && u32 <= 0x303F) // U+3000..U+303F: CJK Symbols and Punctuation Block
            if (ret < 1) ret = 1;
        if (0xAC00 <= u32 && u32 <= 0xD7AF) // U+AC00-U+D7AF: Hangul Syllables Block
            if (ret < 2) ret = 2;
        if (0x1100 <= u32 && u32 <= 0x11FF) // U+1100..U+11FF: Hangul Jamo
            if (ret < 2) ret = 2;
        if (0x3130 <= u32 && u32 <= 0x318F) // U+3130..U+318F: Hangul Compatibility Jamo
            if (ret < 2) ret = 2;
        if (0xA960 <= u32 && u32 <= 0xA97F) // U+A960..U+A97F: Hangul Jamo Extended-A
            if (ret < 2) ret = 2;
        if (0xD7B0 <= u32 && u32 <= 0xD7FF) // U+D7B0..U+D7FF: Hangul Jamo Extended-B
            if (ret < 2) ret = 2;
        if (0xFFA0 <= u32 && u32 <= 0xFFDF) // U+FFA0..U+FFDF: Halfwidth Hangul Filler
            if (ret < 2) ret = 2;
        for (int i = 0; i < skip && *pch; ++i)
            ++pch;
    }
    return ret;
}

// 選択中のフォントが日本語対応か判定する。
bool pdf_is_font_japanese(cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_font_size(cr, 30);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, u8"あ", &extents);
    cairo_restore(cr);
    return !(extents.width < 1 || extents.height < 1);
}

// 選択中のフォントが中国語対応か判定する。
bool pdf_is_font_chinese(cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_font_size(cr, 30);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, u8"沉", &extents);
    cairo_restore(cr);
    return !(extents.width < 1 || extents.height < 1);
}

// 選択中のフォントが韓国語対応か判定する。
bool pdf_is_font_korean(cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_font_size(cr, 30);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, u8"작", &extents);
    cairo_restore(cr);
    return !(extents.width < 1 || extents.height < 1);
}

// 原点を中心として複素数xを回転する。
template <typename T>
inline
complex_rotate(std::complex<T> c, const T& theta)
{
    return c * std::polar<T>(1, theta);
}

// 複素数xを平行移動する。
template <typename T>
inline
complex_translate(std::complex<T> c, const T& x0, const T& y0)
{
    return c + std::complex<T>(x0, y0);
}

// 二つの値がほとんど等しいか？
inline bool is_nearly_equal(double x0, double x1)
{
    return std::fabs(x1 - x0) < 0.25;
}

// 選択中のフォントは等幅フォントか？
bool pdf_is_fixed_pitch_font(cairo_t *cr)
{
    cairo_save(cr);

    cairo_text_extents_t extents;
    cairo_set_font_size(cr, 30);
    cairo_text_extents(cr, "wwww", &extents);

    double x0 = extents.x_advance, x1;
    if (pdf_is_font_japanese(cr))
    {
        cairo_text_extents(cr, u8"目目", &extents);
        x1 = extents.x_advance;
    }
    else if (pdf_is_font_chinese(cr))
    {
        cairo_text_extents(cr, u8"沉沉", &extents);
        x1 = extents.x_advance;
    }
    else if (pdf_is_font_korean(cr))
    {
        cairo_text_extents(cr, u8"작작", &extents);
        x1 = extents.x_advance;
    }
    else
    {
        cairo_text_extents(cr, "iiii", &extents);
        x1 = extents.x_advance;
    }

    cairo_restore(cr);
    return is_nearly_equal(x0, x1);
}

// PDFに出力したときのテキストの幅の合計を返す。
double pdf_get_total_text_width(cairo_t *cr, const char *utf8_text)
{
    std::vector<std::string> chars;
    u8_split_chars(chars, utf8_text);

    double text_width = 0;
    for (size_t ich = 0; ich < chars.size(); ++ich)
    {
        cairo_text_extents_t extents;
        cairo_text_extents(cr, chars[ich].c_str(), &extents);
        text_width += extents.x_advance;
    }

    return text_width;
}

// 小さいカナの縮小率。
#define SMALL_KANA_RATIO 0.55

void pdf_get_h_text_width_and_height(cairo_t *cr, const std::vector<std::string>& chars, double& text_width, double& text_height)
{
    text_width = text_height = 0;
    cairo_text_extents_t extents;
    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);
    for (auto& text_char : chars)
    {
        cairo_text_extents(cr, text_char.c_str(), &extents);
        if (text_height < extents.height)
            text_height = extents.height;
        if (text_height < font_extents.height)
            text_height = font_extents.height;
        text_width += extents.x_advance;
    }
}

void pdf_get_v_text_width_and_height(cairo_t *cr, const std::vector<std::string>& chars, double& text_width, double& text_height)
{
    text_width = text_height = 0;
    cairo_text_extents_t extents;
    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);
    for (auto& text_char : chars)
    {
        cairo_text_extents(cr, text_char.c_str(), &extents);
        if (u8_is_space(text_char.c_str())) // スペースか？
        {
            if (text_width < extents.width)
                text_width = extents.width;
            text_height += extents.x_advance;
        }
        else if (u8_is_small_kana(text_char.c_str())) // 小さいカナか？
        {
            if (text_width < extents.width * SMALL_KANA_RATIO)
                text_width = extents.width * SMALL_KANA_RATIO;
            text_height += extents.height * SMALL_KANA_RATIO;
        }
        else if (u8_is_hyphen_dash(text_char.c_str())) // 横棒か？
        {
            if (text_width < extents.height)
                text_width = extents.height;
            text_height += extents.width;
        }
        else if (u8_is_paren_type_1(text_char.c_str()) ||
                 u8_is_paren_type_2(text_char.c_str()) ||
                 u8_is_paren_type_3(text_char.c_str())) // カッコか？
        {
            if (text_width < extents.height)
                text_width = extents.height;
            text_height += extents.width;
        }
        else
        {
            if (text_width < extents.width)
                text_width = extents.width;
            text_height += extents.height;
        }
    }
}

// Do scaling for drawing horizontal text
bool pdf_scaling_h_text(cairo_t *cr, const char *utf8_text, double width, double height, double& font_size, double& scale_x, double& scale_y, double threshold)
{
    scale_x = scale_y = 1;
    font_size = 10;

    if (!*utf8_text)
        return false;

    // Split characters
    std::vector<std::string> chars;
    u8_split_chars(chars, utf8_text);

    // Adjust the font size and scale
    cairo_text_extents_t extents;
    cairo_font_extents_t font_extents;
    for (;;)
    {
        cairo_set_font_size(cr, font_size);
        double text_width, text_height;
        pdf_get_h_text_width_and_height(cr, chars, text_width, text_height);

        if (font_size >= 10000 || !extents.width || !extents.height)
            return false;

        if (text_width * scale_x < width * 0.9 && text_height * scale_y < height * 0.9)
            font_size *= 1.1;
        else if (threshold < 1.1)
            break;
        else if (text_width * scale_x < width * 0.9)
            scale_x *= 1.1;
        else if (text_height * scale_y < height * 0.9)
            scale_y *= 1.1;
        else
            break;
    }

    std::string text = utf8_text;
    size_t len = u8_len(text.c_str());

    cairo_set_font_size(cr, font_size);

    double text_width, text_height;
    pdf_get_h_text_width_and_height(cr, chars, text_width, text_height);

    if ((text_width * scale_x / len) / (text_height * scale_y) > threshold)
        scale_x = threshold * (text_height * scale_y) * len / text_width;
    if ((text_height * scale_y) / (text_width * scale_x / len) > threshold)
        scale_y = threshold * (text_width * scale_x / len) / text_height;

    return true;
}

// Do scaling for drawing vertical text
bool pdf_scaling_v_text(cairo_t *cr, const char *utf8_text, double width, double height, double& font_size, double& scale_x, double& scale_y, double threshold)
{
    scale_x = scale_y = 1;
    font_size = 10;

    if (!*utf8_text)
        return false;

    // Split characters
    std::vector<std::string> chars;
    u8_split_chars(chars, utf8_text);

    // Adjust the font size and scale
    for (;;)
    {
        double text_width, text_height;
        cairo_set_font_size(cr, font_size);
        pdf_get_v_text_width_and_height(cr, chars, text_width, text_height);

        if (font_size >= 10000 || !text_width || !text_height)
            return false;

        if (text_width * scale_x < width * 0.95 && text_height * scale_y < height * 0.95)
            font_size *= 1.05;
        else if (threshold < 1.1)
            break;
        else if (text_width * scale_x < width * 0.95)
            scale_x *= 1.05;
        else if (text_height * scale_y < height * 0.95)
            scale_y *= 1.05;
        else
            break;
    }

    size_t len = chars.size();

    double text_width, text_height;
    cairo_set_font_size(cr, font_size);
    pdf_get_v_text_width_and_height(cr, chars, text_width, text_height);

    if ((text_width * scale_x) / (text_height * scale_y / len) > threshold)
        scale_x = threshold * (text_height * scale_y / len) / text_width;
    else if ((text_height * scale_y / len) / (text_width * scale_x) > threshold)
        scale_y = threshold * (text_width * scale_x) * len / text_height;

    return true;
}

// 横書き用の文字を描画する。
void pdf_draw_h_char(cairo_t *cr, const char *text_char, double x, double y, double scale_x, double scale_y, cairo_text_extents_t& extents, cairo_font_extents_t& font_extents)
{
    // 補正分。
    y += g_y_adjust;

    // テキストのエクステントを取得
    cairo_text_extents(cr, text_char, &extents);
    cairo_font_extents(cr, &font_extents);

    if (0)
    {
        // ボックスを描画
        cairo_save(cr); // 描画状態を保存
        {
            double x0 = x + extents.x_bearing * scale_x;
            double scaled_width = extents.width * scale_x;
            double scaled_height = font_extents.height * scale_y;

            // 緑色のボックスを描画
            cairo_set_source_rgb(cr, 0, 0.5, 0);  // 緑色
            cairo_rectangle(cr, x0, y, scaled_width, scaled_height);
            cairo_stroke(cr);  // ボックスを描画
        }
        cairo_restore(cr); // 描画状態を元に戻す

        // ベースラインを描画
        cairo_save(cr); // 描画状態を保存
        {
            // 青い線でベースラインを描画
            double x0 = x + extents.x_bearing * scale_x;
            double x1 = x0 + extents.width * scale_x;
            double baseline_y = y + font_extents.ascent * scale_y;
            cairo_set_source_rgb(cr, 0, 0, 1);
            cairo_move_to(cr, x0, baseline_y);
            cairo_line_to(cr, x1, baseline_y);
            cairo_stroke(cr);  // 線を描画
        }
        cairo_restore(cr); // 描画状態を元に戻す
    }

    // テキストを描画
    cairo_save(cr); // 描画状態を保存
    {
        // テキストの基準位置を調整
        double x_pos = x;
        double y_pos = y + font_extents.ascent * scale_y;
        cairo_translate(cr, x_pos, y_pos);  // 指定位置に移動

        // スケーリングを適用
        cairo_scale(cr, scale_x, scale_y);

        // テキストを描画
        cairo_move_to(cr, 0, 0);  // 座標(0, 0)から描画
        cairo_show_text(cr, text_char);
    }
    cairo_restore(cr); // 描画状態を元に戻す
}

// 縦書き用の文字を描画する。
void pdf_draw_v_char(cairo_t *cr, const char *text_char, double x, double y, double scale_x, double scale_y, cairo_text_extents_t& extents, cairo_font_extents_t& font_extents)
{
    // テキストのエクステントを取得
    cairo_text_extents(cr, text_char, &extents);
    cairo_font_extents(cr, &font_extents);

    // 補正分。
    y += g_y_adjust;

    // 句読点なら右にずらす。
    if (u8_is_comma_period(text_char))
        x += extents.width * scale_x * 0.75;

    // 小さいカナなら右にずらす。
    if (u8_is_small_kana(text_char))
    {
        scale_x *= SMALL_KANA_RATIO;
        scale_y *= SMALL_KANA_RATIO;
        x += extents.width * scale_x * 0.5;
    }

    // 横棒なら縦と横を入れ替える。
    if (u8_is_hyphen_dash(text_char)) // 横棒か？
    {
        std::swap(extents.width, extents.height);
        std::swap(extents.x_bearing, extents.y_bearing);
    }

    // カッコなら縦と横を入れ替える。
    if (u8_is_paren_type_1(text_char) ||
        u8_is_paren_type_2(text_char) ||
        u8_is_paren_type_3(text_char)) // カッコか？
    {
        std::swap(extents.width, extents.height);
        std::swap(extents.x_bearing, extents.y_bearing);
    }

    double scaled_width = extents.width * scale_x;
    double scaled_height = extents.height * scale_y;

    if (0)
    {
        // ボックスを描画
        cairo_save(cr); // 描画状態を保存
        {
            double x_pos = x - extents.x_advance * scale_x / 2 + extents.x_bearing * scale_x; // 中央揃え。
            double y_pos = y;

            // 緑色のボックスを描画
            cairo_set_source_rgb(cr, 0, 0.5, 0);  // 緑色
            cairo_rectangle(cr, x_pos, y_pos, scaled_width, scaled_height);
            cairo_stroke(cr);  // ボックスを描画
        }
        cairo_restore(cr); // 描画状態を元に戻す
    }

    // テキストを描画
    cairo_save(cr); // 描画状態を保存
    {
        if (u8_is_hyphen_dash(text_char)) // 横棒か？
        {
            // テキストの基準位置を調整
            double x_pos = x - extents.x_bearing * scale_x - scaled_width / 2;
            double y_pos = y - extents.y_bearing * scale_y;
            cairo_translate(cr, x_pos, y_pos);  // 指定位置に移動

            // スケーリングを適用
            cairo_scale(cr, scale_x, -scale_y);

            // 回転。
            cairo_rotate(cr, -M_PI / 2);

            // テキストを描画
            cairo_move_to(cr, 0, 0);  // 座標(0, 0)から描画
            cairo_show_text(cr, text_char);
        }
        else if (u8_is_paren_type_1(text_char)) // カッコ（タイプ1）か？
        {
            // テキストの基準位置を調整
            double x_pos = x - scaled_width * 0.55 + extents.height * scale_x / 2;
            double y_pos = y - extents.y_bearing * scale_y;
            cairo_translate(cr, x_pos, y_pos);  // 指定位置に移動

            // スケーリングを適用
            cairo_scale(cr, scale_x, scale_y);

            // 回転。
            cairo_rotate(cr, M_PI / 2);

            // テキストを描画
            cairo_move_to(cr, 0, 0);  // 座標(0, 0)から描画
            cairo_show_text(cr, text_char);
        }
        else if (u8_is_paren_type_2(text_char)) // カッコ（タイプ2）か？
        {
            // テキストの基準位置を調整
            double x_pos = x + scaled_width * 0.6 + extents.x_bearing * scale_x;
            double y_pos = y - extents.y_bearing * scale_y;
            cairo_translate(cr, x_pos, y_pos);  // 指定位置に移動

            // スケーリングを適用
            cairo_scale(cr, scale_x, scale_y);

            // 回転。
            cairo_rotate(cr, M_PI / 2);

            // テキストを描画
            cairo_move_to(cr, 0, 0);  // 座標(0, 0)から描画
            cairo_show_text(cr, text_char);
        }
        else if (u8_is_paren_type_3(text_char)) // カッコ（タイプ3）か？
        {
            // テキストの基準位置を調整
            double x_pos = x - scaled_width * 0.55 + extents.y_bearing * scale_x;
            double y_pos = y - extents.y_bearing * scale_y;
            cairo_translate(cr, x_pos, y_pos);  // 指定位置に移動

            // スケーリングを適用
            cairo_scale(cr, scale_x, scale_y);

            // 回転。
            cairo_rotate(cr, M_PI / 2);

            // テキストを描画
            cairo_move_to(cr, 0, 0);  // 座標(0, 0)から描画
            cairo_show_text(cr, text_char);
        }
        else
        {
            // テキストの基準位置を調整
            double x_pos = x - extents.x_advance * scale_x / 2;
            double y_pos = y - extents.y_bearing * scale_y;
            cairo_translate(cr, x_pos, y_pos);  // 指定位置に移動

            // スケーリングを適用
            cairo_scale(cr, scale_x, scale_y);

            // テキストを描画
            cairo_move_to(cr, 0, 0);  // 座標(0, 0)から描画
            cairo_show_text(cr, text_char);
        }
    }
    cairo_restore(cr); // 描画状態を元に戻す
}

// Draw horizontal scaled text
bool pdf_draw_h_text(cairo_t *cr, const char *text, double x0, double y0, double width, double height, double threshold)
{
    if (!*text)
        return false;

    // Calculate scaling and font size
    double font_size, scale_x, scale_y;
    if (!pdf_scaling_h_text(cr, text, width, height, font_size, scale_x, scale_y, threshold))
        return false;

    // Split to characters
    std::vector<std::string> chars;
    u8_split_chars(chars, text);

    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);

    // Draw each character one by one
    double total_text_width = pdf_get_total_text_width(cr, text) * scale_x;
    double text_height = font_extents.height * scale_y;
    double x = x0;
    auto each_blank_width = (width - total_text_width) / (chars.size() + 1);
    for (size_t ich = 0; ich < chars.size(); ++ich)
    {
        x += each_blank_width;

        auto& text_char = chars[ich];

        cairo_text_extents_t extents;
        cairo_text_extents(cr, text_char.c_str(), &extents);

        double y = y0 + (height - font_extents.height * scale_y) / 2;
        pdf_draw_h_char(cr, text_char.c_str(), x, y, scale_x, scale_y, extents, font_extents);

        x += extents.x_advance * scale_x;
    }

    return true;
}

// 縦書きに備えて、半角文字を全角文字に変換する。
std::string u8_locale_map_text(const char *text)
{
    std::wstring wide = wide_from_ansi(text, CP_UTF8);
    mstr_replace_all(wide, L" ", L"\x0001"); // 半角スペース。
    mstr_replace_all(wide, L"　", L"\x0002"); // 全角スペース。
    static WCHAR s_szMapped[1024];
    LCMapStringW(GetUserDefaultLCID(), LCMAP_FULLWIDTH, wide.c_str(), -1, s_szMapped, _countof(s_szMapped));
    wide = s_szMapped;
    mstr_replace_all(wide, L"\x0001", L" "); // 半角スペースを元に戻す。
    mstr_replace_all(wide, L"\x0002", L"　"); // 全角スペースを元に戻す。
    return ansi_from_wide(wide.c_str(), CP_UTF8);
}

// Draw vertical scaled text
bool pdf_draw_v_text(cairo_t *cr, const char *text, double x0, double y0, double width, double height, double threshold)
{
    if (!*text)
        return false;

    // Locale mapping
    std::string mapped_text;
    if (pdf_is_font_japanese(cr) || pdf_is_font_chinese(cr) || pdf_is_font_korean(cr))
        mapped_text = u8_locale_map_text(text);
    else
        mapped_text = text;

    // Calculate scaling and font size
    double font_size, scale_x, scale_y;
    if (!pdf_scaling_v_text(cr, mapped_text.c_str(), width, height, font_size, scale_x, scale_y, threshold))
        return false;

    // Split to characters
    std::vector<std::string> chars;
    u8_split_chars(chars, mapped_text.c_str());

    // get text height
    double text_width, text_height;
    pdf_get_v_text_width_and_height(cr, chars, text_width, text_height);
    text_width *= scale_x;
    text_height *= scale_y;

    // Draw each character one by one
    double y = y0;
    auto each_blank_height = (height - text_height) / (chars.size() + 1);
    while (each_blank_height < font_size / 5)
    {
        scale_x *= 0.95;
        scale_y *= 0.95;

        pdf_get_v_text_width_and_height(cr, chars, text_width, text_height);
        text_width *= scale_x;
        text_height *= scale_y;
        each_blank_height = (height - text_height) / (chars.size() + 1);
    }

    for (size_t ich = 0; ich < chars.size(); ++ich)
    {
        y += each_blank_height;

        auto& text_char = chars[ich];

        cairo_text_extents_t extents;
        cairo_text_extents(cr, text_char.c_str(), &extents);

        cairo_font_extents_t font_extents;
        cairo_font_extents(cr, &font_extents);

        double x = x0 + width / 2;
        pdf_draw_v_char(cr, text_char.c_str(), x, y, scale_x, scale_y, extents, font_extents);

        cairo_text_extents(cr, text_char.c_str(), &extents);

        if (u8_is_space(text_char.c_str()))
            y += extents.x_advance * scale_y;
        else if (u8_is_small_kana(text_char.c_str()))
            y += extents.height * scale_y * SMALL_KANA_RATIO;
        else if (u8_is_hyphen_dash(text_char.c_str()))
            y += extents.width * scale_y;
        else if (u8_is_paren_type_1(text_char.c_str()) || u8_is_paren_type_2(text_char.c_str()) || u8_is_paren_type_3(text_char.c_str()))
            y += extents.width * scale_y;
        else
            y += extents.height * scale_y;
    }

    return true;
}

// Parse command line
bool pdfplaca_parse_cmdline(int argc, _TCHAR **argv)
{
    // Default page size is A4.
    page_size_parse(_T("A4"), &g_page_width, &g_page_height);

    for (int iarg = 1; iarg < argc; ++iarg)
    {
        auto arg = argv[iarg];
        if (_tcsicmp(arg, _T("--help")) == 0 || _tcsicmp(arg, _T("/?")) == 0)
        {
            g_usage = true;
        }
        else if (_tcsicmp(arg, _T("--version")) == 0)
        {
            g_version = true;
        }
        else if (_tcsicmp(arg, _T("--font-list")) == 0)
        {
            g_font_list = true;
        }
        else if (_tcsicmp(arg, _T("--vertical")) == 0)
        {
            g_vertical = true;
        }
        else if (_tcscmp(arg, _T("--text")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            g_out_text = argv[++iarg];
        }
        else if (_tcscmp(arg, _T("-o")) == 0) // output.pdf
        {
            if (iarg + 1 >= argc)
                return false;
            g_out_file = argv[++iarg];
        }
        else if (_tcscmp(arg, _T("--page-size")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            arg = argv[++iarg];
            if (!page_size_parse(arg, &g_page_width, &g_page_height))
                return false;
        }
        else if (_tcsicmp(arg, _T("--portrait")) == 0)
        {
            g_orientation = _T("portrait");
        }
        else if (_tcsicmp(arg, _T("--landscape")) == 0)
        {
            g_orientation = _T("landscape");
        }
        else if (_tcscmp(arg, _T("--margin")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            arg = argv[++iarg];
            _TCHAR *endptr;
            g_margin = _tcstod(arg, &endptr);
            if (*endptr || !std::isnormal(g_margin) || g_margin <= 0)
                return false;
        }
        else if (_tcscmp(arg, _T("--threshold")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            _TCHAR *endptr;
            g_threshold = _tcstod(argv[++iarg], &endptr);
            if (*endptr || !std::isnormal(g_margin) || g_margin < 1.0)
                return false;
        }
        else if (_tcscmp(arg, _T("--y-adjust")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            _TCHAR *endptr;
            g_y_adjust = _tcstod(argv[++iarg], &endptr);
            if (*endptr || std::isinf(g_y_adjust) || std::isnan(g_y_adjust))
                return false;
            g_y_adjust = -pt_from_mm(g_y_adjust);
        }
        else if (_tcscmp(arg, _T("--font")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            g_font_name = argv[++iarg];
        }
        else if (_tcscmp(arg, _T("--text-color")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            uint32_t value = color_value_parse(ansi_from_wide(argv[++iarg]).c_str());
            if (value == uint32_t(-1))
                return false;
            g_text_color = value;
        }
        else if (_tcscmp(arg, _T("--back-color")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            uint32_t value = color_value_parse(ansi_from_wide(argv[++iarg]).c_str());
            if (value == uint32_t(-1))
                return false;
            g_back_color = value;
        }
        else if (_tcscmp(arg, _T("--letters-per-page")) == 0)
        {
            if (iarg + 1 >= argc)
                return false;
            g_letters_per_page = _ttoi(argv[++iarg]);
            if (g_letters_per_page == 0)
                return false;
        }
        else
        {
            return false;
        }
    }

    return true;
}

// 横書きの1ページを描画する。
bool pdfplaca_draw_h_page(cairo_t *cr, const std::vector<std::string>& rows, double page_width, double page_height, double printable_width, double printable_height, double margin)
{
    double y = margin;
    double row_height = (page_height - margin * (rows.size() + 1)) / rows.size();
    for (size_t iRow = 0; iRow < rows.size(); ++iRow)
    {
        // Fill text background
        cairo_save(cr); // Save drawing status
        {
            auto r = get_r_value(g_back_color);
            auto g = get_g_value(g_back_color);
            auto b = get_b_value(g_back_color);
            cairo_set_source_rgb(cr, r / 255.0, g / 255.0, b / 255.0);
            cairo_rectangle(cr, margin, y, printable_width, row_height);
            cairo_fill(cr);
        }
        cairo_restore(cr); // Restore drawing status
        // Draw horizontal text
        {
            auto r = get_r_value(g_text_color);
            auto g = get_g_value(g_text_color);
            auto b = get_b_value(g_text_color);
            cairo_set_source_rgb(cr, r / 255.0, g / 255.0, b / 255.0);
            pdf_draw_h_text(cr, rows[iRow].c_str(), margin, y, printable_width, row_height, g_threshold);
        }
        // Advance
        y += row_height;
        // Advance
        y += margin;
    }

    return true;
}

// 縦書きの1ページを描画する。
bool pdfplaca_draw_v_page(cairo_t *cr, const std::vector<std::string>& rows, double page_width, double page_height, double printable_width, double printable_height, double margin)
{
    double x = 0;
    double row_width = (page_width - margin * (rows.size() + 1)) / rows.size();
    for (size_t iRow = 0; iRow < rows.size(); ++iRow)
    {
        // Advance
        x += margin;
        // Convert X coordinate
        double x0 = (2 * margin + printable_width) - (x + row_width);
        // Fill text background
        cairo_save(cr); // Save drawing status
        {
            auto r = get_r_value(g_back_color);
            auto g = get_g_value(g_back_color);
            auto b = get_b_value(g_back_color);
            cairo_set_source_rgb(cr, r / 255.0, g / 255.0, b / 255.0);
            cairo_rectangle(cr, x0, margin, row_width, printable_height);
            cairo_fill(cr);
        }
        cairo_restore(cr); // Restore drawing status
        // Draw vertical text
        cairo_save(cr); // Save drawing status
        {
            auto r = get_r_value(g_text_color);
            auto g = get_g_value(g_text_color);
            auto b = get_b_value(g_text_color);
            cairo_set_source_rgb(cr, r / 255.0, g / 255.0, b / 255.0);
            pdf_draw_v_text(cr, rows[iRow].c_str(), x0, margin, row_width, printable_height, g_threshold);
        }
        cairo_restore(cr); // Restore drawing status
        // Advance
        x += row_width;
    }

    return true;
}

// ページを描画する。
bool pdfplaca_draw_page(cairo_t *cr, const char *utf8_text, double page_width, double page_height, double printable_width, double printable_height, double margin)
{
    // Split rows
    std::vector<std::string> rows;
    u8_split_by_newlines(rows, utf8_text);

    if (g_vertical) // Vertical writing?
        return pdfplaca_draw_v_page(cr, rows, page_width, page_height, printable_width, printable_height, margin);
    else
        return pdfplaca_draw_h_page(cr, rows, page_width, page_height, printable_width, printable_height, margin);
}

bool pdfplaca_do_it(const _TCHAR *out_file, const _TCHAR *out_text, const _TCHAR *font_name)
{
    // Get page size in points
    double page_width = pt_from_mm(g_page_width), page_height = pt_from_mm(g_page_height);
    printf("page_width: %f pt, page_height: %f pt\n", page_width, page_height);

    // Swap width and height if orientation doesn't match
    if (_tcsicmp(g_orientation, _T("portrait")) == 0)
    {
        if (page_width > page_height)
            std::swap(page_width, page_height);
    }
    else if (_tcsicmp(g_orientation, _T("landscape")) == 0)
    {
        if (page_width < page_height)
            std::swap(page_width, page_height);
    }
    else
    {
        assert(0);
        return false;
    }

    // Get margin in points
    double margin = pt_from_mm(g_margin);

    // Get printable area in points
    double printable_width = page_width - 2 * margin;
    double printable_height = page_height - 2 * margin;

    // Initialize Cairo
#ifdef UNICODE
    std::string filename = ansi_from_wide(out_file, CP_ACP);
#else
    std::string filename = out_file;
#endif
    cairo_surface_t *surface = cairo_pdf_surface_create(filename.c_str(), page_width, page_height);
    cairo_t *cr = cairo_create(surface);

    // Get UTF-8 text
#ifdef UNICODE
    std::string utf8_text = ansi_from_wide(out_text, CP_UTF8);
#else
    std::string utf8_text = out_text;
#endif

    // Choose font and font size
#ifdef UNICODE
    std::string utf8_font_name = ansi_from_wide(font_name, CP_UTF8);
#else
    std::string utf8_font_name = font_name;
#endif
    cairo_select_font_face(cr, utf8_font_name.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    // Display error if text is CJK and font is not CJK
    if (u8_is_japanese_text(utf8_text.c_str()))
    {
        if (!pdf_is_font_japanese(cr))
        {
            utf8_text = u8"   Error:   \nNot Japanese font";
            utf8_font_name = "Arial";
            g_vertical = false;
        }
    }
    else if (u8_is_chinese_text(utf8_text.c_str()))
    {
        if (!pdf_is_font_chinese(cr))
        {
            utf8_text = u8"   Error:   \nNot Chinese font";
            utf8_font_name = "Arial";
            g_vertical = false;
        }
    }
    else if (u8_is_korean_text(utf8_text.c_str()))
    {
        if (!pdf_is_font_korean(cr))
        {
            utf8_text = u8"   Error:   \nNot Korean font";
            utf8_font_name = "Arial";
            g_vertical = false;
        }
    }
    cairo_select_font_face(cr, utf8_font_name.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    // Unescape string
    utf8_text = mstr_unescape(utf8_text.c_str());

    // タブを3スペースに変換する。
    mstr_replace_all(utf8_text, u8"\t", u8"   ");

    // フォントの種類を表示する。
    g_fixed_pitch_font = pdf_is_fixed_pitch_font(cr);
    if (g_fixed_pitch_font)
        printf("fixed-pitch font\n");
    else
        printf("proportional font\n");

    if (0) // ちょっとしたテストを行う。
    {
        //...
    }
    else if (g_letters_per_page == -1) // 1ページの文字数に制限がない？
    {
        // ページ番号を表示する。
        printf("Page %d\n", 1);

        // Draw page (one page only)
        pdfplaca_draw_page(cr, utf8_text.c_str(), page_width, page_height, printable_width, printable_height, margin);
    }
    else if (g_letters_per_page > 0) // 制限がある？
    {
        // Delete spaces
        mstr_replace_all(utf8_text, " ", "");
        mstr_replace_all(utf8_text, "\t", "");
        mstr_replace_all(utf8_text, "\r", "");
        mstr_replace_all(utf8_text, "\n", "");
        mstr_replace_all(utf8_text, u8"　", "");

        // Split characters
        std::vector<std::string> chars;
        u8_split_chars(chars, utf8_text.c_str());

        // Draw pages
        size_t num_page = (chars.size() + g_letters_per_page - 1) / g_letters_per_page;
        for (size_t iPage = 0, iChar = 0; iPage < num_page; ++iPage)
        {
            // ページ番号を表示する。
            printf("Page %d\n", int(iPage + 1));

            // Limit the number of characters per page
            std::string str;
            for (; iChar < (iPage + 1) * g_letters_per_page; ++iChar)
            {
                if (iChar < chars.size())
                    str += chars[iChar];
            }

            // Draw page
            pdfplaca_draw_page(cr, str.c_str(), page_width, page_height, printable_width, printable_height, margin);

            // New page
            cairo_show_page(cr);
        }
    }

    // Clean up
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return true;
}

// フォントを列挙するコールバック関数。
static
INT CALLBACK
EnumFontFamProc(
    const LOGFONTW *plf,
    const TEXTMETRICW *ptm,
    DWORD FontType,
    LPARAM lParam)
{
    auto pList = reinterpret_cast<std::vector<std::wstring> *>(lParam);
    if (plf->lfFaceName[0] == L'@') // 縦書きフォントは無視。
        return TRUE;
    pList->push_back(plf->lfFaceName);
    return TRUE;
}

// フォントを列挙する。
void pdfplaca_list_fonts(void)
{
    HDC hDC = CreateCompatibleDC(NULL);
    std::vector<std::wstring> list;
    EnumFontFamiliesW(hDC, nullptr, (FONTENUMPROCW)EnumFontFamProc, (LPARAM)&list);
    DeleteDC(hDC);

    std::sort(list.begin(), list.end());

    for (auto& entry : list)
    {
        printf("%s\n", ansi_from_wide(entry.c_str(), CP_ACP));
    }
}

int pdfplaca_main(int argc, _TCHAR **argv)
{
    u8_is_japanese_text_unittest();

    if (!pdfplaca_parse_cmdline(argc, argv))
    {
        _ftprintf(stderr, _T("ERROR: Invalid arguments\n"));
        pdfplaca_usage();
        return 1;
    }

    if (g_usage)
    {
        pdfplaca_usage();
        return 0;
    }

    if (g_version)
    {
        pdfplaca_version();
        return 0;
    }

    if (g_font_list)
    {
        pdfplaca_list_fonts();
        return 0;
    }

    if (!pdfplaca_do_it(g_out_file, g_out_text, g_font_name))
        return 1;

    return 0;
}

extern "C"
int _tmain(int argc, _TCHAR **argv)
{
    int ret = pdfplaca_main(argc, argv);

#if (WINVER >= 0x0500) && !defined(NDEBUG)
    // Check handle leak (for Windows only)
    {
        TCHAR szText[MAX_PATH];
        HANDLE hProcess = GetCurrentProcess();
        #if 1
            #undef OutputDebugString
            #define OutputDebugString(str) _fputts((str), stderr);
        #endif
        wnsprintf(szText, _countof(szText),
            TEXT("GDI objects: %ld\n")
            TEXT("USER objects: %ld\n"),
            GetGuiResources(hProcess, GR_GDIOBJECTS),
            GetGuiResources(hProcess, GR_USEROBJECTS));
        OutputDebugString(szText);
    }
#endif

#if defined(_MSC_VER) && !defined(NDEBUG)
    // Detect memory leak (for MSVC only)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    return ret;
}

#ifdef UNICODE
int main(void)
{
    int argc;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int ret = wmain(argc, wargv);
    LocalFree(wargv);
    return ret;
}
#endif
