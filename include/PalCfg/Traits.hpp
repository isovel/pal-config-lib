#pragma once

// How each C++ type reads itself out of a document.
//
// ValueTraits<M> is the extension point: a mod supports a type of its own by
// specialising it, with no registration and no inheritance. Every Read returns
// false to mean "leave the value alone", which is how a missing or unreadable
// setting keeps the default its struct declared.

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <PalCfg/Coerce.hpp>
#include <PalCfg/Diagnostics.hpp>
#include <PalCfg/Field.hpp>
#include <PalCfg/Utf.hpp>
#include <PalCfg/Value.hpp>
#include <PalCfg/Write.hpp>

namespace PalCfg
{
    // Specialise for an enum to give it names. Provide:
    //   static constexpr std::array<std::pair<std::string_view, E>, N> kValues;
    // Names round-trip, and the underlying integer stays acceptable on read.
    template <class E>
    struct EnumNames;

    template <class M, class Enable = void>
    struct ValueTraits;

    // A type whose traits can render it back out. A consumer specialising
    // ValueTraits for a type it only ever reads leaves Write off, and that field
    // is then absent from a generated file.
    template <class M>
    concept Writable = requires(const M& value, std::string& out) { ValueTraits<M>::Write(value, out); };


    template <>
    struct ValueTraits<bool>
    {
        static constexpr Kind kKind = Kind::Bool;

        static bool Read(bool& out, const IValueSource& source, const ReadContext& context)
        {
            if (!CoerceBool(source, out)) return false;
            if (!source.IsBool()) Detail::NoteCoercion(context, source, "a bool");
            return true;
        }

        static void Write(const bool& value, std::string& out) { out += value ? "true" : "false"; }
    };

    // Every integral except bool, which has its own specialisation above.
    template <class M>
    struct ValueTraits<M, std::enable_if_t<std::is_integral_v<M> && !std::is_same_v<M, bool>>>
    {
        static constexpr Kind kKind = Kind::Int;

        static bool Read(M& out, const IValueSource& source, const ReadContext& context)
        {
            std::int64_t wide = 0;
            if (!CoerceInt64(source, wide)) return false;
            if (!source.IsNumber()) Detail::NoteCoercion(context, source, "a whole number");

            wide = static_cast<std::int64_t>(Detail::Clamp(static_cast<double>(wide), context));

            // A value the target type cannot hold keeps the default, so a typo of
            // one digit too many avoids silently wrapping.
            if (wide < static_cast<std::int64_t>(std::numeric_limits<M>::min())) return false;
            if (wide > static_cast<std::int64_t>(std::numeric_limits<M>::max())) return false;

            out = static_cast<M>(wide);
            return true;
        }

        static void Write(const M& value, std::string& out)
        {
            AppendJsonNumber(out, static_cast<double>(value), false);
        }
    };

    template <>
    struct ValueTraits<float>
    {
        static constexpr Kind kKind = Kind::Float;

        static bool Read(float& out, const IValueSource& source, const ReadContext& context)
        {
            double value = 0.0;
            if (!CoerceDouble(source, value)) return false;
            if (!source.IsNumber()) Detail::NoteCoercion(context, source, "a number");

            out = static_cast<float>(Detail::Clamp(value, context));
            return true;
        }

        static void Write(const float& value, std::string& out) { AppendJsonNumber(out, value); }
    };

    template <>
    struct ValueTraits<double>
    {
        static constexpr Kind kKind = Kind::Double;

        static bool Read(double& out, const IValueSource& source, const ReadContext& context)
        {
            double value = 0.0;
            if (!CoerceDouble(source, value)) return false;
            if (!source.IsNumber()) Detail::NoteCoercion(context, source, "a number");

            out = Detail::Clamp(value, context);
            return true;
        }

        static void Write(const double& value, std::string& out)
        {
            AppendJsonNumber(out, value, true);
        }
    };

    template <>
    struct ValueTraits<std::string>
    {
        static constexpr Kind kKind = Kind::String;

        static bool Read(std::string& out, const IValueSource& source, const ReadContext& context)
        {
            if (!CoerceUtf8(source, out)) return false;
            if (!source.IsString()) Detail::NoteCoercion(context, source, "text");
            return true;
        }

        static void Write(const std::string& value, std::string& out)
        {
            AppendJsonString(out, value);
        }
    };

    template <>
    struct ValueTraits<std::wstring>
    {
        static constexpr Kind kKind = Kind::String;

        static bool Read(std::wstring& out, const IValueSource& source, const ReadContext& context)
        {
            std::string utf8;
            if (!CoerceUtf8(source, utf8)) return false;
            if (!source.IsString()) Detail::NoteCoercion(context, source, "text");

            out = Utf8ToWide(utf8);
            return true;
        }

        static void Write(const std::wstring& value, std::string& out)
        {
            AppendJsonString(out, WideToUtf8(value));
        }
    };

    namespace Detail
    {
        // Keeping a default is a success whose value happens to be unchanged.
        // Collapsing it into "unreadable" would make the loader warn about a file
        // that is perfectly correct - every list in PerkyPals' shipped config whose
        // default is already empty arrives as [].
        // Renders one element per entry. A list stays on one line: PerkyPals'
        // longest is six short names, and one line per list keeps a config file
        // readable at a glance.
        template <class Element, class Range>
        void WriteElements(const Range& values, std::string& out)
        {
            out += '[';

            bool first = true;
            for (const auto& value : values)
            {
                if (!first) out += ", ";
                first = false;
                ValueTraits<Element>::Write(value, out);
            }

            out += ']';
        }

        enum class ElementsOutcome
        {
            Assign,
            KeepDefault,
            Unreadable,
        };

        // Shared by vector and set: reads every element that its own traits accept,
        // skipping the rest. One bad entry costs that entry alone.
        template <class Element, class Emit>
        ElementsOutcome ReadElements(const IValueSource& source, const ReadContext& context, Emit&& emit)
        {
            std::size_t accepted = 0;

            const auto Accept = [&](const IValueSource& element) {
                Element value{};
                if (!ValueTraits<Element>::Read(value, element, context)) return;
                emit(std::move(value));
                ++accepted;
            };

            // An empty result either leaves the default standing or is taken at face
            // value, per the field's own flag. PerkyPals needs both: most of its
            // lists want the default back, while idleActions treats empty as a real
            // answer.
            const auto Outcome = [&] {
                if (accepted > 0) return ElementsOutcome::Assign;
                return context.meta.keepDefaultIfEmpty ? ElementsOutcome::KeepDefault
                                                       : ElementsOutcome::Assign;
            };

            if (source.IsString())
            {
                // One separated string stands in for a list, which is how iaho
                // spelled its id lists.
                std::string text;
                if (!source.AsUtf8(text)) return ElementsOutcome::Unreadable;

                ForEachSeparatedToken(text, [&](std::string_view token) {
                    WithStringSource(token, Accept);
                });

                NoteCoercion(context, source, "a list");
                return Outcome();
            }

            if (!source.IsArray()) return ElementsOutcome::Unreadable;

            for (std::size_t i = 0; i < source.Size(); ++i) source.Element(i, Accept);

            return Outcome();
        }

        template <class M>
        struct ElementKindOf
        {
            static constexpr Kind kValue = Kind::Unknown;
        };

        template <class E, class A>
        struct ElementKindOf<std::vector<E, A>>
        {
            static constexpr Kind kValue = ValueTraits<E>::kKind;
        };

        template <class E, class H, class Eq, class A>
        struct ElementKindOf<std::unordered_set<E, H, Eq, A>>
        {
            static constexpr Kind kValue = ValueTraits<E>::kKind;
        };
    } // namespace Detail

    template <class Element>
    struct ValueTraits<std::vector<Element>>
    {
        static constexpr Kind kKind = Kind::List;

        static bool Read(std::vector<Element>& out, const IValueSource& source, const ReadContext& context)
        {
            std::vector<Element> parsed;
            const auto outcome = Detail::ReadElements<Element>(
                source, context, [&](Element value) { parsed.push_back(std::move(value)); });

            if (outcome == Detail::ElementsOutcome::Unreadable) return false;
            if (outcome == Detail::ElementsOutcome::Assign) out = std::move(parsed);

            return true;
        }
        static void Write(const std::vector<Element>& value, std::string& out)
            requires Writable<Element>
        {
            Detail::WriteElements<Element>(value, out);
        }
    };

    template <class Element, class Hash, class Equal, class Alloc>
    struct ValueTraits<std::unordered_set<Element, Hash, Equal, Alloc>>
    {
        static constexpr Kind kKind = Kind::Set;

        using Set = std::unordered_set<Element, Hash, Equal, Alloc>;

        static bool Read(Set& out, const IValueSource& source, const ReadContext& context)
        {
            Set parsed;
            const auto outcome = Detail::ReadElements<Element>(
                source, context, [&](Element value) { parsed.insert(std::move(value)); });

            if (outcome == Detail::ElementsOutcome::Unreadable) return false;
            if (outcome == Detail::ElementsOutcome::Assign) out = std::move(parsed);

            return true;
        }
        // Sorted, since a hash set has no order of its own and a generated file
        // must not churn between runs.
        static void Write(const Set& value, std::string& out)
            requires Writable<Element>
        {
            std::vector<std::string> rendered;
            rendered.reserve(value.size());
            for (const auto& element : value)
            {
                ValueTraits<Element>::Write(element, rendered.emplace_back());
            }
            std::sort(rendered.begin(), rendered.end());

            out += '[';
            for (std::size_t i = 0; i < rendered.size(); ++i)
            {
                if (i > 0) out += ", ";
                out += rendered[i];
            }
            out += ']';
        }
    };

    // An absent key never reaches Read, so the member keeps its declared
    // nullopt. A present key that reads cleanly engages the option.
    template <class Inner>
    struct ValueTraits<std::optional<Inner>>
    {
        static constexpr Kind kKind = Kind::Optional;

        static bool Read(std::optional<Inner>& out, const IValueSource& source, const ReadContext& context)
        {
            Inner value{};
            if (!ValueTraits<Inner>::Read(value, source, context)) return false;

            out = std::move(value);
            return true;
        }
        // An unset option writes null, which reads back as unset: the file says
        // "no answer here" in the one spelling JSON has for it.
        static void Write(const std::optional<Inner>& value, std::string& out)
            requires Writable<Inner>
        {
            if (!value.has_value()) out += "null";
            else ValueTraits<Inner>::Write(*value, out);
        }
    };

    template <class M>
    struct ValueTraits<M, std::enable_if_t<std::is_enum_v<M>>>
    {
        static constexpr Kind kKind = Kind::Enum;

        static bool Read(M& out, const IValueSource& source, const ReadContext& context)
        {
            if (source.IsString())
            {
                std::string name;
                if (!source.AsUtf8(name)) return false;

                // Case-insensitive, so a user writing "Quiet" gets the value that
                // the schema spells "quiet".
                for (const auto& [candidate, value] : EnumNames<M>::kValues)
                {
                    if (!EqualsIgnoringCase(name, candidate)) continue;
                    out = value;
                    return true;
                }
                return false; // an unrecognised name keeps the default
            }

            std::int64_t ordinal = 0;
            if (!source.AsInt64(ordinal)) return false;

            for (const auto& [candidate, value] : EnumNames<M>::kValues)
            {
                (void)candidate;
                if (static_cast<std::int64_t>(value) != ordinal) continue;

                out = value;

                // The name is this setting's canonical spelling, so arriving as a
                // number is worth a note.
                Detail::NoteCoercion(context, source, "a named value");
                return true;
            }
            return false; // a number outside the enum keeps the default
        }

        // The name is the canonical spelling, so a value with no name falls back
        // to its ordinal and still reads back.
        static void Write(const M& value, std::string& out)
        {
            for (const auto& [candidate, known] : EnumNames<M>::kValues)
            {
                if (known != value) continue;
                AppendJsonString(out, candidate);
                return;
            }

            AppendJsonNumber(out, static_cast<double>(value), false);
        }
    };

    // The type-erased thunk for one (struct, member type) pair. Templated on the
    // types alone, never on the member-pointer value, so the instantiation count
    // stays at one per pair however many fields share a type. The member pointer
    // travels as data in FieldRuntime::boundMemberPtr.
    template <class T, class M>
    struct OpsFor
    {
        static bool Parse(void* base,
                          const void* boundMemberPtr,
                          const IValueSource& source,
                          const ReadContext& context)
        {
            const auto memberPtr = *static_cast<M T::* const*>(boundMemberPtr);
            return ValueTraits<M>::Read(static_cast<T*>(base)->*memberPtr, source, context);
        }

        // The body is guarded because taking this function's address below
        // instantiates it for every M, ordered or not.
        static void SwapIfInverted(void* base, const void* lowerMemberPtr, const void* upperMemberPtr)
        {
            if constexpr (std::is_arithmetic_v<M>)
            {
                const auto lower = *static_cast<M T::* const*>(lowerMemberPtr);
                const auto upper = *static_cast<M T::* const*>(upperMemberPtr);

                T& target = *static_cast<T*>(base);
                if (target.*lower <= target.*upper) return;

                const M held = target.*lower;
                target.*lower = target.*upper;
                target.*upper = held;
            }
        }

        static void Format(const void* base, const void* boundMemberPtr, std::string& out)
        {
            if constexpr (Writable<M>)
            {
                const auto memberPtr = *static_cast<M T::* const*>(boundMemberPtr);
                ValueTraits<M>::Write(static_cast<const T*>(base)->*memberPtr, out);
            }
        }

        // Ordering is what the swap needs, so types without it get no hook and
        // .SwapIfInverted has nothing to call.
        static constexpr auto kSwapHook = std::is_arithmetic_v<M> ? &SwapIfInverted : nullptr;

        static constexpr auto kFormatHook = Writable<M> ? &Format : nullptr;

        static inline constexpr FieldOps kInstance{ValueTraits<M>::kKind, &Parse, kSwapHook, kFormatHook, Detail::ElementKindOf<M>::kValue};
    };
} // namespace PalCfg
