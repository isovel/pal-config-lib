#include <PalCfg/Utf.hpp>

#include <cstdint>

namespace PalCfg
{
    namespace
    {
        constexpr char32_t kReplacement = 0xFFFD;
        constexpr char32_t kMaxCodePoint = 0x10FFFF;

        constexpr bool IsSurrogate(char32_t cp)
        {
            return cp >= 0xD800 && cp <= 0xDFFF;
        }

        constexpr bool IsContinuation(unsigned char byte)
        {
            return (byte & 0xC0) == 0x80;
        }

        // Decodes one code point starting at `i`, advancing it past what was
        // consumed. A malformed sequence yields U+FFFD and consumes at least one
        // byte, so decoding always makes progress.
        char32_t DecodeUtf8(std::string_view utf8, std::size_t& i)
        {
            const auto Byte = [&](std::size_t at) { return static_cast<unsigned char>(utf8[at]); };

            const unsigned char lead = Byte(i);

            std::size_t extra = 0;
            char32_t cp = 0;

            if (lead < 0x80)
            {
                ++i;
                return lead;
            }
            else if ((lead & 0xE0) == 0xC0)
            {
                extra = 1;
                cp = lead & 0x1Fu;
            }
            else if ((lead & 0xF0) == 0xE0)
            {
                extra = 2;
                cp = lead & 0x0Fu;
            }
            else if ((lead & 0xF8) == 0xF0)
            {
                extra = 3;
                cp = lead & 0x07u;
            }
            else
            {
                ++i; // a continuation byte or 0xF8+ cannot start a sequence
                return kReplacement;
            }

            // Bytes i+1 through i+extra must all be present. When they are not,
            // consume the maximal subpart - the lead plus whatever valid
            // continuations did arrive - and emit a single replacement. Unicode
            // recommends one U+FFFD per maximal subpart, so a truncated sequence
            // reads as one damaged character.
            if (i + extra >= utf8.size())
            {
                std::size_t consumed = 1;
                while (i + consumed < utf8.size() && IsContinuation(Byte(i + consumed))) ++consumed;
                i += consumed;
                return kReplacement;
            }

            for (std::size_t k = 1; k <= extra; ++k)
            {
                if (!IsContinuation(Byte(i + k)))
                {
                    // Same maximal-subpart rule: the lead and the k-1 good
                    // continuations are one damaged character, and the offending
                    // byte is left to be decoded on its own terms.
                    i += k;
                    return kReplacement;
                }
                cp = (cp << 6) | (Byte(i + k) & 0x3Fu);
            }

            i += extra + 1;

            // Overlong encodings, surrogates and out-of-range values are all
            // invalid, and accepting them is how UTF-8 decoders become exploits.
            const bool overlong = (extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
                                  (extra == 3 && cp < 0x10000);
            if (overlong || IsSurrogate(cp) || cp > kMaxCodePoint) return kReplacement;

            return cp;
        }

        void AppendWide(std::wstring& out, char32_t cp)
        {
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (cp <= 0xFFFF)
                {
                    out.push_back(static_cast<wchar_t>(cp));
                    return;
                }
                const char32_t offset = cp - 0x10000;
                out.push_back(static_cast<wchar_t>(0xD800 + (offset >> 10)));
                out.push_back(static_cast<wchar_t>(0xDC00 + (offset & 0x3FF)));
            }
            else
            {
                out.push_back(static_cast<wchar_t>(cp));
            }
        }

        void AppendUtf8(std::string& out, char32_t cp)
        {
            const auto Push = [&](char32_t byte) { out.push_back(static_cast<char>(byte)); };

            if (cp < 0x80)
            {
                Push(cp);
            }
            else if (cp < 0x800)
            {
                Push(0xC0 | (cp >> 6));
                Push(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000)
            {
                Push(0xE0 | (cp >> 12));
                Push(0x80 | ((cp >> 6) & 0x3F));
                Push(0x80 | (cp & 0x3F));
            }
            else
            {
                Push(0xF0 | (cp >> 18));
                Push(0x80 | ((cp >> 12) & 0x3F));
                Push(0x80 | ((cp >> 6) & 0x3F));
                Push(0x80 | (cp & 0x3F));
            }
        }
    } // namespace

    std::wstring Utf8ToWide(std::string_view utf8)
    {
        std::wstring out;
        out.reserve(utf8.size());

        for (std::size_t i = 0; i < utf8.size();) AppendWide(out, DecodeUtf8(utf8, i));

        return out;
    }

    std::string WideToUtf8(std::wstring_view wide)
    {
        std::string out;
        out.reserve(wide.size());

        for (std::size_t i = 0; i < wide.size(); ++i)
        {
            char32_t cp = static_cast<char32_t>(static_cast<std::uint32_t>(wide[i]));

            if constexpr (sizeof(wchar_t) == 2)
            {
                const bool isHighSurrogate = cp >= 0xD800 && cp <= 0xDBFF;
                if (isHighSurrogate && i + 1 < wide.size())
                {
                    const char32_t low = static_cast<char32_t>(static_cast<std::uint32_t>(wide[i + 1]));
                    if (low >= 0xDC00 && low <= 0xDFFF)
                    {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        ++i;
                    }
                    else
                    {
                        cp = kReplacement; // high surrogate with no pair
                    }
                }
                else if (IsSurrogate(cp))
                {
                    cp = kReplacement; // lone surrogate
                }
            }
            else if (IsSurrogate(cp) || cp > kMaxCodePoint)
            {
                cp = kReplacement;
            }

            AppendUtf8(out, cp);
        }

        return out;
    }
} // namespace PalCfg
