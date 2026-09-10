#include <PalCfg/Jsonc.hpp>

#include <vector>

namespace PalCfg
{
    namespace
    {
        constexpr bool IsSpace(char c)
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        constexpr bool StartsLineComment(const std::string& text, std::size_t i)
        {
            return text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/';
        }

        constexpr bool StartsBlockComment(const std::string& text, std::size_t i)
        {
            return text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*';
        }

        // Index just past a comment starting at `i`.
        std::size_t SkipComment(const std::string& text, std::size_t i)
        {
            if (StartsLineComment(text, i))
            {
                i += 2;
                while (i < text.size() && text[i] != '\n') ++i;
                return i;
            }

            i += 2; // past "/*"
            while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) ++i;
            return i + 1 < text.size() ? i + 2 : text.size();
        }

        // Index just past the string literal whose opening quote is at `i`.
        std::size_t SkipString(const std::string& text, std::size_t i)
        {
            ++i; // past the opening quote
            while (i < text.size())
            {
                if (text[i] == '\\')
                {
                    i += 2; // the backslash and whatever it escapes
                    continue;
                }
                if (text[i] == '"') return i + 1;
                ++i;
            }
            return text.size(); // unterminated; let the parser report it
        }

        // Next character that carries meaning at or after `i`, skipping
        // whitespace and comments. A comma followed by a closing brace or
        // bracket is trailing however much commentary sits between them.
        std::size_t NextSignificant(const std::string& text, std::size_t i)
        {
            while (i < text.size())
            {
                if (IsSpace(text[i]))
                {
                    ++i;
                    continue;
                }
                if (StartsLineComment(text, i) || StartsBlockComment(text, i))
                {
                    i = SkipComment(text, i);
                    continue;
                }
                return i;
            }
            return std::string::npos;
        }

        bool StripBom(std::string& text)
        {
            const auto Byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };

            if (text.size() < 3) return false;
            if (Byte(0) != 0xEF || Byte(1) != 0xBB || Byte(2) != 0xBF) return false;

            text.erase(0, 3);
            return true;
        }
    } // namespace

    SanitiseResult SanitiseJsonc(std::string& text)
    {
        SanitiseResult result{};
        result.bomRemoved = StripBom(text);

        // Collect first, erase after, so the indices stay valid during the scan.
        std::vector<std::size_t> trailing;

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '"')
            {
                i = SkipString(text, i) - 1; // the loop's ++i lands past the literal
                continue;
            }
            if (StartsLineComment(text, i) || StartsBlockComment(text, i))
            {
                i = SkipComment(text, i) - 1;
                continue;
            }
            if (text[i] != ',') continue;

            const auto next = NextSignificant(text, i + 1);
            if (next == std::string::npos) continue;
            if (text[next] == '}' || text[next] == ']') trailing.push_back(i);
        }

        // Back to front, for the same reason.
        for (auto it = trailing.rbegin(); it != trailing.rend(); ++it) text.erase(*it, 1);
        result.trailingCommasRemoved = trailing.size();

        return result;
    }
} // namespace PalCfg
