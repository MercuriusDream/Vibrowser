#pragma once
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Forward declare QuickJS types
struct JSRuntime;
struct JSContext;
struct JSModuleDef;

namespace clever::js {

// Callback type for console.log output
using ConsoleCallback = std::function<void(const std::string& level, const std::string& message)>;

// Callback type for module fetching (returns module source code or empty)
using ModuleFetcher = std::function<std::optional<std::string>(const std::string& module_url)>;

class JSEngine {
public:
    JSEngine();
    ~JSEngine();

    // Non-copyable, movable
    JSEngine(const JSEngine&) = delete;
    JSEngine& operator=(const JSEngine&) = delete;
    JSEngine(JSEngine&&) noexcept;
    JSEngine& operator=(JSEngine&&) noexcept;

    // Execute JavaScript code, returns result as string (or empty on error)
    std::string evaluate(const std::string& code, const std::string& filename = "<script>");

    // Execute JavaScript code as an ES module (supports import/export syntax)
    std::string evaluate_module(const std::string& code, const std::string& filename = "<module>");

    // Check if last evaluation had an error
    bool has_error() const { return has_error_; }
    const std::string& last_error() const { return last_error_; }

    // Console output
    void set_console_callback(ConsoleCallback cb);
    const std::vector<std::string>& console_output() const { return console_output_; }

    // Set module fetcher for dynamic module imports
    void set_module_fetcher(ModuleFetcher fetcher);

    // Access the raw QuickJS context (for bindings to add properties)
    JSContext* context() const { return ctx_; }
    JSRuntime* runtime() const { return rt_; }

private:
    JSRuntime* rt_ = nullptr;
    JSContext* ctx_ = nullptr;
    bool has_error_ = false;
    std::string last_error_;
    std::vector<std::string> console_output_;
    ConsoleCallback console_callback_;
    ModuleFetcher module_fetcher_;
    std::map<std::string, JSModuleDef*> module_cache_;

    void setup_console();
    void clear_error();

    // Allow the console callback trampoline to access internals
    friend JSEngine* get_engine_from_ctx(JSContext* ctx);
    friend struct ConsoleTrampoline;
    friend struct ModuleLoaderTrampoline;
};

} // namespace clever::js
