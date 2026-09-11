#pragma once

// Regenerating a mod's config file from its schema.
//
// A mod builds a small host-native tool around GenerateMain and runs it at build
// time, so the shipped config.default.json is a build artifact and its comments
// can never drift from the .Help() text they came from.

#include <array>
#include <cstddef>
#include <iostream>
#include <ostream>
#include <string>

#include <PalCfg/Document.hpp>
#include <PalCfg/Field.hpp>
#include <PalCfg/File.hpp>
#include <PalCfg/Write.hpp>

namespace PalCfg
{
    struct GenerateResult
    {
        bool changed = false;
        bool backedUp = false;
        std::string error;

        bool Ok() const { return error.empty(); }
    };

    // Renders `defaults` over whatever `path` already holds, keeping any key the
    // schema does not claim. A file that cannot be parsed stops the generation:
    // overwriting it would discard settings that a fixed typo would have restored.
    template <class T, std::size_t N>
    GenerateResult GenerateFile(const std::array<FieldRuntime, N>& fields,
                                const T& defaults,
                                const std::string& path,
                                const WriteOptions& options = {})
    {
        GenerateResult result;

        std::string existing;
        Document previous;

        if (ReadFileText(path, existing) && !existing.empty())
        {
            if (!previous.Parse(std::move(existing)))
            {
                result.error = path + ": " + previous.Error().message;
                return result;
            }
            if (!previous.Root().IsObject())
            {
                result.error = path + ": the document's root is not an object";
                return result;
            }
        }

        const std::string rendered =
            RenderDocument(fields, defaults, previous.Ok() ? &previous.Root() : nullptr, options);

        const auto written = WriteFileIfChanged(path, rendered);
        result.changed = written.written;
        result.backedUp = written.backedUp;
        result.error = written.error;

        return result;
    }

    // The body of a mod's generator tool:
    //
    //     int main(int argc, char** argv)
    //     {
    //         return PalCfg::GenerateMain(argc, argv, kFields, Settings{});
    //     }
    template <class T, std::size_t N>
    int GenerateMain(int argc,
                     const char* const* argv,
                     const std::array<FieldRuntime, N>& fields,
                     const T& defaults,
                     std::ostream& log = std::cout,
                     const WriteOptions& options = {})
    {
        if (argc != 2)
        {
            log << "usage: palcfg-gen <path to config file>\n";
            return 2;
        }

        const std::string path = argv[1];
        const auto result = GenerateFile(fields, defaults, path, options);

        if (!result.Ok())
        {
            log << result.error << "\n";
            return 1;
        }

        if (!result.changed) log << path << " is already up to date\n";
        else if (result.backedUp) log << "wrote " << path << ", keeping the old one as " << path << ".bak\n";
        else log << "wrote " << path << "\n";

        return 0;
    }
} // namespace PalCfg
