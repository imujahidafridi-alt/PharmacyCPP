#pragma once
#include <cstdint>
#include <QString>

namespace core {

class Money {
public:
    constexpr Money() noexcept : m_paisa(0) {}
    constexpr explicit Money(int64_t paisa) noexcept : m_paisa(paisa) {}
    
    static Money fromRupees(double rupees);
    static Money fromPaisa(int64_t paisa) { return Money(paisa); }

    int64_t paisa() const noexcept { return m_paisa; }
    double toRupees() const noexcept { return static_cast<double>(m_paisa) / 100.0; }

    QString formatted(bool includeCurrency = true) const;

    Money operator+(const Money& other) const noexcept { return Money(m_paisa + other.m_paisa); }
    Money operator-(const Money& other) const noexcept { return Money(m_paisa - other.m_paisa); }
    Money operator*(double factor) const noexcept;
    Money operator/(int64_t divisor) const noexcept;

    Money& operator+=(const Money& other) noexcept { m_paisa += other.m_paisa; return *this; }
    Money& operator-=(const Money& other) noexcept { m_paisa -= other.m_paisa; return *this; }

    bool operator==(const Money& other) const noexcept { return m_paisa == other.m_paisa; }
    bool operator!=(const Money& other) const noexcept { return m_paisa != other.m_paisa; }
    bool operator<(const Money& other) const noexcept { return m_paisa < other.m_paisa; }
    bool operator<=(const Money& other) const noexcept { return m_paisa <= other.m_paisa; }
    bool operator>(const Money& other) const noexcept { return m_paisa > other.m_paisa; }
    bool operator>=(const Money& other) const noexcept { return m_paisa >= other.m_paisa; }

    bool isZero() const noexcept { return m_paisa == 0; }
    bool isPositive() const noexcept { return m_paisa > 0; }
    bool isNegative() const noexcept { return m_paisa < 0; }

private:
    int64_t m_paisa{0};
};

} // namespace core
