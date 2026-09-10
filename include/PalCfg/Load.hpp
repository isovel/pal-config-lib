#pragma once

// Fills a settings struct from a parsed document, one field at a time.
//
// A field whose key is absent, or whose value the field's traits decline, keeps
// whatever the struct already held. Since a load starts from a freshly
// default-constructed struct, that means deleting a key from the file restores
// its default. One bad field costs that field alone.

#include <array>
#include <cstddef>

#include <PalCfg/Field.hpp>
#include <PalCfg/Value.hpp>

namespace PalCfg
{
    template <class T, std::size_t N>
    void LoadFields(const std::array<FieldRuntime, N>& fields, const IValueSource& root, T& out)
    {
        for (const auto& field : fields)
        {
            if (field.ops == nullptr || field.ops->parse == nullptr) continue;
            if (field.meta.key == nullptr) continue;

            root.Child(field.meta.key, [&](const IValueSource& value) {
                field.ops->parse(&out, field.boundMemberPtr, value, field.meta);
            });
        }
    }
} // namespace PalCfg
