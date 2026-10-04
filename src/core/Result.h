#pragma once
#include <variant>
#include <string>
#include <optional>
#include <utility>

namespace core {

template <typename T, typename E = std::string>
class Result {
public:
    static Result ok(T value) {
        return Result(std::move(value), true);
    }

    static Result err(E error) {
        return Result(std::move(error), false);
    }

    bool isOk() const noexcept {
        return std::holds_alternative<T>(m_data);
    }

    bool isErr() const noexcept {
        return std::holds_alternative<E>(m_data);
    }

    const T& value() const {
        return std::get<T>(m_data);
    }

    T& value() {
        return std::get<T>(m_data);
    }

    const E& error() const {
        return std::get<E>(m_data);
    }

    E& error() {
        return std::get<E>(m_data);
    }

    T valueOr(T fallback) const {
        if (isOk()) return value();
        return fallback;
    }

private:
    Result(T val, bool) : m_data(std::move(val)) {}
    Result(E err, bool) : m_data(std::move(err)) {}

    std::variant<T, E> m_data;
};

// Specialization for void
template <typename E>
class Result<void, E> {
public:
    static Result ok() {
        return Result(std::nullopt);
    }

    static Result err(E error) {
        return Result(std::move(error));
    }

    bool isOk() const noexcept {
        return !m_error.has_value();
    }

    bool isErr() const noexcept {
        return m_error.has_value();
    }

    const E& error() const {
        return *m_error;
    }

private:
    explicit Result(std::nullopt_t) : m_error(std::nullopt) {}
    explicit Result(E err) : m_error(std::move(err)) {}

    std::optional<E> m_error;
};

} // namespace core
