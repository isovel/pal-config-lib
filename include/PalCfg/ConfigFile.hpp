#pragma once

// One config file, kept live.
//
// A poll reads the file only when its stamp has moved, and reloads only when the
// bytes have actually changed, so a mod writing its own file never triggers
// itself. DynamicPals' FileWatcher polled at 1 Hz for the same reason: a watcher
// thread on a Windows handle would deliver notifications on a thread that must
// not touch the game.
//
// The live settings are held behind an atomic shared pointer, so a game thread
// reading them never sees a half-applied reload.

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <PalCfg/Diagnostics.hpp>
#include <PalCfg/Field.hpp>
#include <PalCfg/File.hpp>
#include <PalCfg/Generate.hpp>
#include <PalCfg/Load.hpp>

namespace PalCfg
{
    // Get() is safe from any thread. Load(), Tick() and Save() MUST be called
    // from one thread, which for a UE4SS mod is the game thread.
    template <class T, std::size_t N>
    class ConfigFile
    {
      public:
        using Clock = std::chrono::steady_clock;

        ConfigFile(const std::array<FieldRuntime, N>& fields, std::string path)
            : m_fields(fields), m_path(std::move(path)), m_live(std::make_shared<const T>())
        {
        }

        const std::string& Path() const { return m_path; }

        void SetSink(IDiagnosticSink& sink) { m_sink = &sink; }
        void SetInterval(Clock::duration interval) { m_interval = interval; }
        void SetWriteOptions(const WriteOptions& options) { m_options = options; }

        // Called from Tick() with the settings that have just gone live.
        void SetOnReload(std::function<void(const T&)> callback)
        {
            m_onReload = std::move(callback);
        }

        std::shared_ptr<const T> Get() const { return m_live.load(); }

        // Reads the file now, whatever the interval says. Returns false when the
        // file is missing or unreadable, leaving the settings already live.
        bool Load()
        {
            std::string text;
            if (!ReadFileText(m_path, text))
            {
                m_stamp = StatFile(m_path);
                Report(Severity::Warning, "cannot read " + m_path + "; using the defaults");
                return false;
            }

            return Apply(std::move(text));
        }

        // Returns true when this call put new settings live.
        bool Tick(Clock::time_point now)
        {
            if (now - m_lastPoll < m_interval) return false;
            m_lastPoll = now;

            const FileStamp stamp = StatFile(m_path);
            if (stamp == m_stamp) return false;

            std::string text;
            if (!ReadFileText(m_path, text))
            {
                m_stamp = stamp;
                return false;
            }

            // The bytes are the last word, so a rewrite that changed nothing -
            // this mod saving its own file included - is not a reload.
            if (text == m_lastText)
            {
                m_stamp = stamp;
                return false;
            }

            if (!Apply(std::move(text))) return false;

            if (m_onReload) m_onReload(*Get());
            return true;
        }

        // Renders `value` back to the file, keeping every key the schema does not
        // claim, and puts it live. The write is not seen as an edit.
        bool Save(const T& value)
        {
            const auto result = GenerateFile(m_fields, value, m_path, m_options);
            if (!result.Ok())
            {
                Report(Severity::Error, result.error);
                return false;
            }

            m_live.store(std::make_shared<const T>(value));
            ReadFileText(m_path, m_lastText);
            m_stamp = StatFile(m_path);

            return true;
        }

      private:
        // Records the text whether or not it loaded, so a file that cannot be
        // parsed is reported once rather than on every poll.
        bool Apply(std::string text)
        {
            auto next = std::make_shared<T>();
            IDiagnosticSink& sink = m_sink != nullptr ? *m_sink : m_null;

            const LoadResult result = LoadFromText(m_fields, text, *next, sink);

            m_lastText = std::move(text);
            m_stamp = StatFile(m_path);

            if (!result.parsed) return false;

            m_live.store(std::move(next));
            return true;
        }

        void Report(Severity severity, std::string message)
        {
            if (m_sink == nullptr) return;
            m_sink->Report({severity, {}, std::move(message)});
        }

        const std::array<FieldRuntime, N>& m_fields;
        std::string m_path;

        std::atomic<std::shared_ptr<const T>> m_live;

        std::string m_lastText;
        FileStamp m_stamp{};

        Clock::duration m_interval = std::chrono::seconds{1};
        Clock::time_point m_lastPoll{};

        std::function<void(const T&)> m_onReload;

        IDiagnosticSink* m_sink = nullptr;
        NullSink m_null;

        WriteOptions m_options{};
    };
} // namespace PalCfg
