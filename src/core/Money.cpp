#include "core/Money.h"
#include <cmath>
#include <QLocale>

namespace core {

Money Money::fromRupees(double rupees)
{
    int64_t p = static_cast<int64_t>(std::round(rupees * 100.0));
    return Money(p);
}

Money Money::operator*(double factor) const noexcept
{
    return Money(static_cast<int64_t>(std::round(m_paisa * factor)));
}

Money Money::operator/(int64_t divisor) const noexcept
{
    if (divisor == 0) return Money(0);
    return Money(m_paisa / divisor);
}

QString Money::formatted(bool includeCurrency) const
{
    double val = toRupees();
    QString formattedNum = QLocale().toString(val, 'f', (m_paisa % 100 == 0) ? 0 : 2);
    if (includeCurrency) {
        return QString("Rs. %1").arg(formattedNum);
    }
    return formattedNum;
}

} // namespace core
