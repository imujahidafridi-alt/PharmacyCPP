#pragma once
#include <QByteArray>
#include <QString>
#include "core/Result.h"
#include "core/Errors.h"

namespace printing {

class ESCPOSPrinter {
public:
    static QByteArray initPrinter();
    static QByteArray alignCenter();
    static QByteArray alignLeft();
    static QByteArray alignRight();
    static QByteArray boldOn();
    static QByteArray boldOff();
    static QByteArray doubleHeightOn();
    static QByteArray doubleHeightOff();
    static QByteArray lineFeed(int lines = 1);
    static QByteArray cutPaper();
    static QByteArray openCashDrawer();
    static QByteArray horizontalLine(int width = 42);

    // Send raw bytes directly to Windows printer port (via winspool API)
    static core::Result<void, core::AppError> sendRawToPrinter(const QString& printerName, const QByteArray& data);
};

} // namespace printing
