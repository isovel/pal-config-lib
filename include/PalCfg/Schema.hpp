#pragma once

// Compile-time description of a settings struct.
//
// A schema is pure metadata: it names each field, points at the member it fills,
// and carries the presentation details a config file's comments and a settings
// menu both need. It is allocation-free, and it stays clear of json.hpp and
// Windows.h, which is what lets one declaration serve the parser, the defaults,
// the file writer and the menu while consumers keep their translation units free
// of a JSON parser.
//
// Declare a schema at namespace scope as `inline constexpr`:
//
//     inline constexpr auto kSchema = PalCfg::Schema<Settings>("MyMod")
//         .Field("rate", &Settings::Rate);
//     inline constexpr auto kFields = kSchema.Flatten();
//
// Flatten() borrows addresses into the schema object, so the schema must outlive
// the descriptors and must have static storage duration for Flatten() to be
// usable in a constant expression. `inline` gives every translation unit the
// same object.

#include <array>
#include <cstddef>
#include <tuple>
#include <utility>

#include <PalCfg/Field.hpp>
#include <PalCfg/Traits.hpp>

namespace PalCfg
{
    template <class T, class... Ms>
    class Schema
    {
      public:
        using Struct = T;

        static constexpr std::size_t Count = sizeof...(Ms);

        constexpr explicit Schema(const char* modId) : m_modId(modId) {}

        constexpr Schema(const char* modId,
                         std::tuple<Ms T::*...> members,
                         std::array<FieldMeta, Count> meta,
                         bool caseInsensitiveKeys,
                         std::size_t lastSpan)
            : m_modId(modId), m_members(members), m_meta(meta),
              m_caseInsensitiveKeys(caseInsensitiveKeys), m_lastSpan(lastSpan)
        {
        }

        // The only builder method that grows the type.
        template <class M>
        constexpr auto Field(const char* key, M T::* memberPtr) const -> Schema<T, Ms..., M>
        {
            std::array<FieldMeta, Count + 1> next{};
            for (std::size_t i = 0; i < Count; ++i) next[i] = m_meta[i];
            next[Count].key = key;

            return Schema<T, Ms..., M>{m_modId,
                                       std::tuple_cat(m_members, std::tuple<M T::*>{memberPtr}),
                                       next,
                                       m_caseInsensitiveKeys,
                                       1};
        }

        // Two fields of one type that bound a single quantity. Modifiers that
        // follow apply to both, and .SwapIfInverted() restores their order when a
        // file has them the wrong way round - an invariant of the pair that
        // .Range() cannot express. PerkyPals did this by hand after parsing.
        template <class M>
        constexpr auto FieldPair(const char* lowerKey,
                                 M T::* lowerPtr,
                                 const char* upperKey,
                                 M T::* upperPtr) const -> Schema<T, Ms..., M, M>
        {
            std::array<FieldMeta, Count + 2> next{};
            for (std::size_t i = 0; i < Count; ++i) next[i] = m_meta[i];

            next[Count].key = lowerKey;
            next[Count].pairPartner = Count + 1;
            next[Count + 1].key = upperKey;

            return Schema<T, Ms..., M, M>{
                m_modId,
                std::tuple_cat(m_members, std::tuple<M T::*, M T::*>{lowerPtr, upperPtr}),
                next,
                m_caseInsensitiveKeys,
                2};
        }

        // MUST follow a .FieldPair(), which the compiler enforces: a pair's lower
        // field names its partner, and anything else indexes past the array, a
        // hard error during constant evaluation. Without the check this would
        // quietly mark a field the loader then skips, so the invariant a caller
        // asked for would go missing with no signal.
        constexpr Schema SwapIfInverted() const
        {
            auto meta = m_meta;
            const bool followsPair = Count >= 2 && meta[Count - 2].pairPartner == Count - 1;

            meta[followsPair ? Count - 2 : Count].swapIfInverted = true;
            return Schema{m_modId, m_members, meta, m_caseInsensitiveKeys, m_lastSpan};
        }

        // Modifiers return the SAME type, so a long fluent chain costs no extra
        // template instantiations - only .Field() and .FieldPair() grow the pack.
        constexpr Schema Label(const char* text) const
        {
            return WithLastField([&](FieldMeta& field) { field.label = text; });
        }

        constexpr Schema Help(const char* text) const
        {
            return WithLastField([&](FieldMeta& field) { field.help = text; });
        }

        constexpr Schema Range(double lo, double hi) const
        {
            return WithLastField([&](FieldMeta& field) {
                field.min = lo;
                field.max = hi;
                field.hasRange = true;
            });
        }

        constexpr Schema Alias(const char* key) const
        {
            return WithLastField([&](FieldMeta& field) {
                // More than kMaxAliases writes past the array, which is a hard
                // error during constant evaluation. A schema is always constexpr,
                // so this surfaces at compile time, and it holds on builds with
                // exceptions disabled.
                field.aliases[field.aliasCount++] = key;
            });
        }

        constexpr Schema KeepDefaultIfEmpty() const
        {
            return WithLastField([](FieldMeta& field) { field.keepDefaultIfEmpty = true; });
        }

        constexpr Schema Hidden() const
        {
            return WithLastField([](FieldMeta& field) { field.hidden = true; });
        }

        constexpr Schema Advanced() const
        {
            return WithLastField([](FieldMeta& field) { field.advanced = true; });
        }

        // Schema-level, so it may appear anywhere in the chain and applies to every
        // field. dynamic-pals and iaho both lower-cased keys before matching.
        constexpr Schema CaseInsensitiveKeys() const
        {
            return Schema{m_modId, m_members, m_meta, true, m_lastSpan};
        }

        constexpr std::array<FieldRuntime, Count> Flatten() const
        {
            std::array<FieldRuntime, Count> out{};
            Fill(out, std::make_index_sequence<Count>{});
            return out;
        }

        constexpr const char* ModId() const { return m_modId; }

      private:
        constexpr FieldMeta StampedMeta(std::size_t index) const
        {
            FieldMeta meta = m_meta[index];
            meta.caseInsensitiveKeys = m_caseInsensitiveKeys;
            return meta;
        }

        template <class Fn>
        constexpr Schema WithLastField(Fn&& apply) const
        {
            static_assert(Count > 0, "a modifier needs a preceding .Field()");

            // Applies across the whole of the last declaration, so a modifier
            // after .FieldPair() reaches both of its fields.
            auto meta = m_meta;
            for (std::size_t i = Count - m_lastSpan; i < Count; ++i) apply(meta[i]);

            return Schema{m_modId, m_members, meta, m_caseInsensitiveKeys, m_lastSpan};
        }

        template <std::size_t... Is>
        constexpr void Fill(std::array<FieldRuntime, Count>& out, std::index_sequence<Is...>) const
        {
            ((out[Is] = FieldRuntime{StampedMeta(Is),
                                     &OpsFor<T, std::tuple_element_t<Is, std::tuple<Ms...>>>::kInstance,
                                     &std::get<Is>(m_members)}),
             ...);
        }

        const char* m_modId = nullptr;
        std::tuple<Ms T::*...> m_members{};
        std::array<FieldMeta, Count> m_meta{};
        bool m_caseInsensitiveKeys = false;

        // How many fields the most recent declaration added: one for .Field(), two
        // for .FieldPair().
        std::size_t m_lastSpan = 0;
    };
} // namespace PalCfg
