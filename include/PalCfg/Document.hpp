#pragma once

// A parsed JSONC document, and the only place a config file's bytes turn into
// values. Holds the parse tree behind a pimpl so json.hpp stays inside
// src/core/JsonBackend.cpp.

#include <memory>
#include <string>

#include <PalCfg/Jsonc.hpp>
#include <PalCfg/Value.hpp>

namespace PalCfg
{
    struct ParseError
    {
        // Byte offset into the sanitised text, as reported by the parser.
        std::size_t byteOffset = 0;
        std::string message;
    };

    class Document
    {
      public:
        Document();
        ~Document();

        Document(Document&&) noexcept;
        Document& operator=(Document&&) noexcept;

        Document(const Document&) = delete;
        Document& operator=(const Document&) = delete;

        // Sanitises `text`, then parses it with comments allowed. Returns false on
        // a document-level failure, where nothing can be extracted and a caller
        // MUST keep whatever settings it already had.
        bool Parse(std::string text);

        bool Ok() const;
        const ParseError& Error() const;

        // What sanitising had to fix. Worth reporting as a note, so a user learns
        // their file had a stray comma.
        const SanitiseResult& Sanitised() const;

        // Valid only while Ok(). On a failed parse this is a null node, so a
        // caller that ignores the return value reads defaults and stays safe.
        const IValueSource& Root() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace PalCfg
