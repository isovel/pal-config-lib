#include <PalCfg/Write.hpp>

#include <array>
#include <charconv>
#include <cstdio>
#include <system_error>

namespace PalCfg
{
    namespace
    {
        void AppendEscape(std::string& out, char c)
        {
            static constexpr char kHex[] = "0123456789abcdef";

            out += "\\u00";
            out += kHex[(static_cast<unsigned char>(c) >> 4) & 0xF];
            out += kHex[static_cast<unsigned char>(c) & 0xF];
        }

        template <class Value>
        void AppendShortest(std::string& out, Value value, bool fractional)
        {
            std::array<char, 32> buffer{};
            const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if (error != std::errc{}) return;

            const std::string_view text{buffer.data(), static_cast<std::size_t>(end - buffer.data())};
            out += text;

            const bool whole = text.find_first_of(".eE") == std::string_view::npos;
            if (fractional && whole) out += ".0";
        }
    } // namespace

    namespace Detail
    {
    void AppendComment(std::string& out, std::string_view text, std::string_view indent)
    {
        for (;;)
        {
            const auto newline = text.find('\n');
            const std::string_view line = text.substr(0, newline);

            out += indent;
            out += line.empty() ? "//" : "// ";
            out += line;
            out += '\n';

            if (newline == std::string_view::npos) return;
            text = text.substr(newline + 1);
        }
    }
    } // namespace Detail

    void AppendJsonString(std::string& out, std::string_view text)
    {
        out += '"';
        for (const char c : text)
        {
            switch (c)
            {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                default:
                    // Everything else passes through, so UTF-8 stays readable in
                    // the file rather than becoming \u escapes.
                    if (static_cast<unsigned char>(c) < 0x20) AppendEscape(out, c);
                    else out += c;
                    break;
            }
        }
        out += '"';
    }


    void AppendJsonNumber(std::string& out, double value, bool fractional)
    {
        AppendShortest(out, value, fractional);
    }

    void AppendJsonNumber(std::string& out, float value)
    {
        AppendShortest(out, value, true);
    }
} // namespace PalCfg::Detail
