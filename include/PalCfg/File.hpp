#pragma once

// Putting a generated file on disk without losing what was there.
//
// A config file is a user's own work. Replacing one keeps the previous copy
// beside it, and a file whose content has not changed is left untouched, so a
// regeneration that decides nothing never disturbs a timestamp - or, once hot
// reload arrives, triggers itself.

#include <cstdint>
#include <string>
#include <string_view>

namespace PalCfg
{
    struct WriteFileResult
    {
        // False when the file already held this content, and when writing failed.
        bool written = false;

        // Whether a previous file was moved to "<path>.bak".
        bool backedUp = false;

        // Empty on success.
        std::string error;

        bool Ok() const { return error.empty(); }
    };

    // Enough of a file's identity to tell "nobody has touched this" cheaply. The
    // content still decides whether a reload happens: a stamp only says whether
    // the file is worth reading again.
    struct FileStamp
    {
        bool exists = false;
        std::uint64_t size = 0;
        std::int64_t modified = 0;

        bool operator==(const FileStamp&) const = default;
    };

    FileStamp StatFile(const std::string& path);

    // Writes through a temporary in the same directory, then renames, so a reader
    // sees either the whole old file or the whole new one.
    WriteFileResult WriteFileIfChanged(const std::string& path, std::string_view content);

    // Returns false when the file cannot be read, leaving `out` empty.
    bool ReadFileText(const std::string& path, std::string& out);
} // namespace PalCfg
