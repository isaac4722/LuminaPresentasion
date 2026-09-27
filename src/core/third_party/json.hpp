// Placeholder de nlohmann/json. Reemplazar por el header real antes de compilar.
// Origen: https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp
//
// Mientras tanto, una API mínima compatible con el uso en el cimiento,
// suficiente para que el código C++ compile (NO para producción).
#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace nlohmann {
class json {
public:
    json() = default;
    static json parse(const std::string& s, bool = true, bool = true) {
        (void)s;
        return json{};
    }
    static json parse(const char* s) { (void)s; return json{}; }
    static json array() { json j; j.is_array_ = true; return j; }
    static json object() { json j; return j; }

    bool is_array() const { return is_array_; }
    bool is_object() const { return !is_array_; }
    bool is_discarded() const { return false; }
    bool is_null() const { return false; }
    bool contains(const std::string&) const { return false; }

    template <typename T> T get() const { return T{}; }

    std::string value(const std::string& key, const char* def) const {
        (void)key; return def ? def : "";
    }
    std::string value(const std::string& key, const std::string& def) const {
        (void)key; return def;
    }
    bool value(const std::string& key, bool def) const { (void)key; return def; }
    int value(const std::string& key, int def) const { (void)key; return def; }
    std::uint32_t value(const std::string& key, std::uint32_t def) const { (void)key; return def; }

    const json& operator[](const std::string&) const { static json d; return d; }
    json& operator[](const std::string&) { return *this; }
    const json& operator[](size_t) const { static json d; return d; }
    json& operator[](size_t) { return *this; }

    json& operator=(const std::string& s) { (void)s; return *this; }
    json& operator=(const char* s) { (void)s; return *this; }
    json& operator=(bool b) { (void)b; return *this; }
    json& operator=(int i) { (void)i; return *this; }
    json& operator=(const json&) { return *this; }

    template <typename T> void push_back(T&& v) { (void)v; }
    std::string dump(int = -1) const { return "{}"; }

    // Soporte para inicialización con {}
    json(std::initializer_list<std::pair<const std::string, json>>) {}
    json(std::initializer_list<json>) : is_array_(true) {}

private:
    bool is_array_ = false;
};
} // namespace nlohmann
