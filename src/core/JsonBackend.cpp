// The one translation unit that knows about nlohmann. Everything above it sees
// only IValueSource.

#include <PalCfg/Document.hpp>

#include <nlohmann/json.hpp>

namespace PalCfg
{
    namespace
    {
        class JsonSource final : public IValueSource
        {
          public:
            explicit JsonSource(const nlohmann::json& node) : m_node(&node) {}

            bool IsNull() const override { return m_node->is_null(); }
            bool IsBool() const override { return m_node->is_boolean(); }
            bool IsNumber() const override { return m_node->is_number(); }
            bool IsString() const override { return m_node->is_string(); }
            bool IsArray() const override { return m_node->is_array(); }
            bool IsObject() const override { return m_node->is_object(); }

            bool AsBool(bool& out) const override
            {
                if (!m_node->is_boolean()) return false;
                out = m_node->get<bool>();
                return true;
            }

            bool AsInt64(std::int64_t& out) const override
            {
                if (!m_node->is_number_integer()) return false;
                out = m_node->get<std::int64_t>();
                return true;
            }

            bool AsDouble(double& out) const override
            {
                if (!m_node->is_number()) return false;
                out = m_node->get<double>();
                return true;
            }

            bool AsUtf8(std::string& out) const override
            {
                if (!m_node->is_string()) return false;
                out = m_node->get<std::string>();
                return true;
            }

            std::size_t Size() const override
            {
                return m_node->is_array() || m_node->is_object() ? m_node->size() : 0;
            }

            bool Element(std::size_t index, const Visitor& visit) const override
            {
                if (!m_node->is_array() || index >= m_node->size()) return false;
                visit(JsonSource{(*m_node)[index]});
                return true;
            }

            bool Child(std::string_view key, const Visitor& visit) const override
            {
                if (!m_node->is_object()) return false;

                const auto found = m_node->find(std::string{key});
                if (found == m_node->end()) return false;

                visit(JsonSource{*found});
                return true;
            }

            void ForEachKey(const std::function<void(std::string_view)>& visit) const override
            {
                if (!m_node->is_object()) return;
                for (auto it = m_node->begin(); it != m_node->end(); ++it) visit(it.key());
            }

            void Dump(std::string& out) const override { out = m_node->dump(); }

          private:
            // A pointer, so a source stays assignable. Always non-null; it points
            // at a node owned by the Document, or at the shared null node.
            const nlohmann::json* m_node;
        };

        const nlohmann::json& NullNode()
        {
            static const nlohmann::json kNull{};
            return kNull;
        }
    } // namespace

    struct Document::Impl
    {
        // The sanitised bytes are kept because the parse tree's strings are copies
        // but the error offset refers back into this text.
        std::string text;
        nlohmann::json root;
        SanitiseResult sanitised;
        ParseError error;
        bool ok = false;

        JsonSource rootView{NullNode()};
    };

    Document::Document() : m_impl(std::make_unique<Impl>()) {}
    Document::~Document() = default;
    Document::Document(Document&&) noexcept = default;
    Document& Document::operator=(Document&&) noexcept = default;

    bool Document::Parse(std::string text)
    {
        auto& impl = *m_impl;

        impl.ok = false;
        impl.error = {};
        impl.root = nullptr;
        impl.rootView = JsonSource{NullNode()};

        impl.text = std::move(text);
        impl.sanitised = SanitiseJsonc(impl.text);

        try
        {
            // allow_exceptions and ignore_comments, so a hand-edited file may
            // carry // and /* */ notes.
            impl.root = nlohmann::json::parse(impl.text, nullptr, true, true);
        }
        catch (const nlohmann::json::parse_error& error)
        {
            impl.error.byteOffset = error.byte;
            impl.error.message = error.what();
            return false;
        }

        impl.rootView = JsonSource{impl.root};
        impl.ok = true;
        return true;
    }

    bool Document::Ok() const
    {
        return m_impl->ok;
    }

    const ParseError& Document::Error() const
    {
        return m_impl->error;
    }

    const SanitiseResult& Document::Sanitised() const
    {
        return m_impl->sanitised;
    }

    const IValueSource& Document::Root() const
    {
        return m_impl->rootView;
    }
} // namespace PalCfg
