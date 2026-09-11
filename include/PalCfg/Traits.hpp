#pragma once

// How each C++ type reads itself out of a document.
//
// ValueTraits<M> is the extension point: a mod supports a type of its own by
// specialising it, with no registration and no inheritance. Every Read returns
// false to mean "leave the value alone", which is how a missing or unreadable
// setting keeps the default its struct declared.

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

namespace PalCfg
{
    // Specialise for an enum to give it names. Provide:
    //   static constexpr std::array<std::pair<std::string_view, E>, N> kValues;
    // Names round-trip, and the underlying integer stays acceptable on read.
    template <class E>
    struct EnumNames;

    template <class M, class Enable = void>
    struct ValueTraits;


    template <>
    struct ValueTraits<bool>
    {
        static constexpr Kind kKind = Kind::Bool;

        static bool Read(bool& out, const IValueSource& source, const ReadContext& context)
        {
            if (!Detail::CoerceBool(source, out)) return false;
            if (!source.IsBool()) Detail::NoteCoercion(context, source, "a bool");
            return true;
        }
    };

    // Every integral except bool, which has its own specialisation above.
    template <class M>
    struct ValueTraits<M, std::enable_if_t<std::is_integral_v<M> && !std::is_same_v<M, bool>>>
    {
        static constexpr Kind kKind = Kind::Int;

        static bool Read(M& out, const IValueSource& source, const ReadContext& context)
        {
            std::int64_t wide = 0;
            if (!Detail::CoerceInt64(source, wide)) return false;
            if (!source.IsNumber()) Detail::NoteCoercion(context, source, "a whole number");

            wide = static_cast<std::int64_t>(Detail::Clamp(static_cast<double>(wide), context));

            // A value the target type cannot hold keeps the default, so a typo of
            // one digit too many avoids silently wrapping.
            if (wide < static_cast<std::int64_t>(std::numeric_limits<M>::min())) return false;
            if (wide > static_cast<std::int64_t>(std::numeric_limits<M>::max())) return false;

            out = static_cast<M>(wide);
            return true;
        }
    };

    template <>
    struct ValueTraits<float>
    {
        static constexpr Kind kKind = Kind::Float;

        static bool Read(float& out, const IValueSource& source, const ReadContext& context)
        {
            double value = 0.0;
            if (!Detail::CoerceDouble(source, value)) return false;
            if (!source.IsNumber()) Detail::NoteCoercion(context, source, "a number");

            out = static_cast<float>(Detail::Clamp(value, context));
            return true;
        }
    };

    template <>
    struct ValueTraits<double>
    {
        static constexpr Kind kKind = Kind::Double;

        static bool Read(double& out, const IValueSource& source, const ReadContext& context)
        {
            double value = 0.0;
            if (!Detail::CoerceDouble(source, value)) return false;
            if (!source.IsNumber()) Detail::NoteCoercion(context, source, "a number");

            out = Detail::Clamp(value, context);
            return true;
        }
    };

    template <>
    struct ValueTraits<std::string>
    {
        static constexpr Kind kKind = Kind::String;

        static bool Read(std::string& out, const IValueSource& source, const ReadContext& context)
        {
            if (!Detail::CoerceUtf8(source, out)) return false;
            if (!source.IsString()) Detail::NoteCoercion(context, source, "text");
            return true;
        }
    };

    template <>
    struct ValueTraits<std::wstring>
    {
        static constexpr Kind kKind = Kind::String;

        static bool Read(std::wstring& out, const IValueSource& source, const ReadContext& context)
        {
            std::string utf8;
            if (!Detail::CoerceUtf8(source, utf8)) return false;
            if (!source.IsString()) Detail::NoteCoercion(context, source, "text");

            out = Utf8ToWide(utf8);
            return true;
        }
    };

    namespace Detail
    {
        // Keeping a default is a success whose value happens to be unchanged.
        // Collapsing it into "unreadable" would make the loader warn about a file
        // that is perfectly correct - every list in PerkyPals' shipped config whose
        // default is already empty arrives as [].
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
                    if (!Detail::EqualsIgnoringCase(name, candidate)) continue;
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

        // Ordering is what the swap needs, so types without it get no hook and
        // .SwapIfInverted has nothing to call.
        static constexpr auto kSwapHook = std::is_arithmetic_v<M> ? &SwapIfInverted : nullptr;

        static inline constexpr FieldOps kInstance{ValueTraits<M>::kKind, &Parse, kSwapHook};
    };
} // namespace PalCfg
