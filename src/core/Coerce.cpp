#include <PalCfg/Coerce.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace PalCfg
{
    namespace
    {
        constexpr bool IsSpace(char c)
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
            while (!text.empty() && IsSpace(text.back())) text.remove_suffix(1);
            return text;
        }

        // Parses a whole numeric string. Anything trailing makes it a failure, so
        // "0.5x" keeps the default in place of quietly reading 0.5.
        bool ParseNumber(std::string_view text, double& out)
        {
            const std::string_view trimmed = Trim(text);
            if (trimmed.empty()) return false;

            // from_chars for double is unimplemented in libstdc++ 12, so this goes
            // through strtod and checks that the whole token was consumed.
            const std::string owned{trimmed};
            char* end = nullptr;
            errno = 0;
            const double value = std::strtod(owned.c_str(), &end);

            if (end != owned.c_str() + owned.size()) return false;
            if (!std::isfinite(value)) return false;

            out = value;
            return true;
        }

        // A single string, presented as a document node.
        class StringSource final : public IValueSource
        {
          public:
            explicit StringSource(std::string_view text) : m_text(text) {}

            bool IsNull() const override { return false; }
            bool IsBool() const override { return false; }
            bool IsNumber() const override { return false; }
            bool IsString() const override { return true; }
            bool IsArray() const override { return false; }
            bool IsObject() const override { return false; }

            bool AsBool(bool&) const override { return false; }
            bool AsInt64(std::int64_t&) const override { return false; }
            bool AsDouble(double&) const override { return false; }

            bool AsUtf8(std::string& out) const override
            {
                out.assign(m_text);
                return true;
            }

            std::size_t Size() const override { return 0; }
            bool Element(std::size_t, const Visitor&) const override { return false; }
            bool Child(std::string_view, const Visitor&) const override { return false; }
            void ForEachKey(const std::function<void(std::string_view)>&) const override {}

            void Dump(std::string& out) const override
            {
                out = '"';
                out.append(m_text);
                out += '"';
            }

          private:
            std::string_view m_text;
        };
    } // namespace

    bool EqualsIgnoringCase(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size()) return false;

        const auto Lower = [](char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        };

        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (Lower(a[i]) != Lower(b[i])) return false;
        }
        return true;
    }

    bool CoerceBool(const IValueSource& source, bool& out)
    {
        if (source.AsBool(out)) return true;

        double number = 0.0;
        if (source.AsDouble(number))
        {
            out = number != 0.0;
            return true;
        }

        std::string text;
        if (!source.AsUtf8(text)) return false;

        // iaho's IsTruthy accepted 1/true/yes/on; the negatives are its mirror.
        static constexpr std::array<std::string_view, 4> kTrue{"true", "yes", "on", "1"};
        static constexpr std::array<std::string_view, 4> kFalse{"false", "no", "off", "0"};

        const std::string_view trimmed = Trim(text);
        for (const auto word : kTrue)
        {
            if (!EqualsIgnoringCase(trimmed, word)) continue;
            out = true;
            return true;
        }
        for (const auto word : kFalse)
        {
            if (!EqualsIgnoringCase(trimmed, word)) continue;
            out = false;
            return true;
        }

        // Any other number spelled as a string, so "2" behaves as 2 does.
        if (!ParseNumber(trimmed, number)) return false;
        out = number != 0.0;
        return true;
    }

    bool CoerceInt64(const IValueSource& source, std::int64_t& out)
    {
        if (source.AsInt64(out)) return true;

        double number = 0.0;
        if (!source.AsDouble(number))
        {
            std::string text;
            if (!source.AsUtf8(text)) return false;
            if (!ParseNumber(text, number)) return false;
        }

        // Truncates toward zero, as dynamic-pals' SafeGetInt did.
        const double truncated = std::trunc(number);
        if (truncated < static_cast<double>(std::numeric_limits<std::int64_t>::min())) return false;
        if (truncated > static_cast<double>(std::numeric_limits<std::int64_t>::max())) return false;

        out = static_cast<std::int64_t>(truncated);
        return true;
    }

    bool CoerceDouble(const IValueSource& source, double& out)
    {
        if (source.AsDouble(out)) return true;

        std::string text;
        if (!source.AsUtf8(text)) return false;

        return ParseNumber(text, out);
    }

    bool CoerceUtf8(const IValueSource& source, std::string& out)
    {
        if (source.AsUtf8(out)) return true;

        bool boolean = false;
        if (source.AsBool(boolean))
        {
            out = boolean ? "true" : "false";
            return true;
        }

        std::int64_t integer = 0;
        if (source.AsInt64(integer))
        {
            out = std::to_string(integer);
            return true;
        }

        double number = 0.0;
        if (!source.AsDouble(number)) return false;

        // Shortest round-trip form, so 0.25 does not arrive as "0.250000".
        std::array<char, 32> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
        if (result.ec != std::errc{}) return false;

        out.assign(buffer.data(), result.ptr);
        return true;
    }

    void WithStringSource(std::string_view text, const IValueSource::Visitor& visit)
    {
        visit(StringSource{text});
    }

    void ForEachSeparatedToken(std::string_view text,
                               const std::function<void(std::string_view)>& visit)
    {
        std::size_t start = 0;
        while (start <= text.size())
        {
            const auto end = text.find_first_of(",;", start);
            const auto piece = text.substr(start, end == std::string_view::npos
                                                      ? std::string_view::npos
                                                      : end - start);

            const std::string_view token = Trim(piece);
            if (!token.empty()) visit(token);

            if (end == std::string_view::npos) return;
            start = end + 1;
        }
    }
} // namespace PalCfg
