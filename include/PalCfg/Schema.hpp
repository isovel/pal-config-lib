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
                         std::array<FieldMeta, Count> meta)
            : m_modId(modId), m_members(members), m_meta(meta)
        {
        }

        // The only builder method that grows the type.
        template <class M>
        constexpr auto Field(const char* key, M T::* memberPtr) const -> Schema<T, Ms..., M>
        {
            std::array<FieldMeta, Count + 1> next{};
            for (std::size_t i = 0; i < Count; ++i) next[i] = m_meta[i];
            next[Count].key = key;

            return Schema<T, Ms..., M>{
                m_modId, std::tuple_cat(m_members, std::tuple<M T::*>{memberPtr}), next};
        }

        // Modifiers return the SAME type, so a long fluent chain costs no extra
        // template instantiations - only .Field() grows the parameter pack.
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

        constexpr std::array<FieldRuntime, Count> Flatten() const
        {
            std::array<FieldRuntime, Count> out{};
            Fill(out, std::make_index_sequence<Count>{});
            return out;
        }

        constexpr const char* ModId() const { return m_modId; }

      private:
        template <class Fn>
        constexpr Schema WithLastField(Fn&& apply) const
        {
            static_assert(Count > 0, "a modifier needs a preceding .Field()");

            auto meta = m_meta;
            apply(meta[Count - 1]);
            return Schema{m_modId, m_members, meta};
        }

        template <std::size_t... Is>
        constexpr void Fill(std::array<FieldRuntime, Count>& out, std::index_sequence<Is...>) const
        {
            ((out[Is] = FieldRuntime{m_meta[Is],
                                     &OpsFor<T, std::tuple_element_t<Is, std::tuple<Ms...>>>::kInstance,
                                     &std::get<Is>(m_members)}),
             ...);
        }

        const char* m_modId = nullptr;
        std::tuple<Ms T::*...> m_members{};
        std::array<FieldMeta, Count> m_meta{};
    };
} // namespace PalCfg
