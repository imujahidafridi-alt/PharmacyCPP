#include "printing/ESCPOSPrinter.h"
#ifdef _WIN32
#include <windows.h>
#include <winspool.h>
#endif

namespace printing {

QByteArray ESCPOSPrinter::initPrinter()
{
    return QByteArray("\x1B\x40", 2); // ESC @
}

QByteArray ESCPOSPrinter::alignCenter()
{
    return QByteArray("\x1B\x61\x01", 3); // ESC a 1
}

QByteArray ESCPOSPrinter::alignLeft()
{
    return QByteArray("\x1B\x61\x00", 3); // ESC a 0
}

QByteArray ESCPOSPrinter::alignRight()
{
    return QByteArray("\x1B\x61\x02", 3); // ESC a 2
}

QByteArray ESCPOSPrinter::boldOn()
{
    return QByteArray("\x1B\x45\x01", 3); // ESC E 1
}

QByteArray ESCPOSPrinter::boldOff()
{
    return QByteArray("\x1B\x45\x00", 3); // ESC E 0
}

QByteArray ESCPOSPrinter::doubleHeightOn()
{
    return QByteArray("\x1D\x21\x01", 3); // GS ! 1
}

QByteArray ESCPOSPrinter::doubleHeightOff()
{
    return QByteArray("\x1D\x21\x00", 3); // GS ! 0
}

QByteArray ESCPOSPrinter::lineFeed(int lines)
{
    QByteArray ba;
    for (int i = 0; i < lines; ++i) {
        ba.append('\n');
    }
    return ba;
}

QByteArray ESCPOSPrinter::cutPaper()
{
    return QByteArray("\x1D\x56\x41\x03", 4); // GS V 65 3
}

QByteArray ESCPOSPrinter::openCashDrawer()
{
    // ESC p m t1 t2: pulse on pin 2 (m=0)
    return QByteArray("\x1B\x70\x00\x19\xFA", 5);
}

QByteArray ESCPOSPrinter::horizontalLine(int width)
{
    return QByteArray(width, '-') + "\n";
}

core::Result<void, core::AppError> ESCPOSPrinter::sendRawToPrinter(const QString& printerName, const QByteArray& data)
{
#ifdef _WIN32
    if (printerName.isEmpty()) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::PrinterUnavailable, "No printer specified.")
        );
    }

    HANDLE hPrinter = NULL;
    std::wstring pNameW = printerName.toStdWString();
    if (!OpenPrinterW(const_cast<LPWSTR>(pNameW.c_str()), &hPrinter, NULL)) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::PrinterUnavailable, "Could not open printer handle.")
        );
    }

    DOC_INFO_1W docInfo;
    wchar_t docTitle[] = L"Pharmacy Receipt";
    docInfo.pDocName = docTitle;
    docInfo.pOutputFile = NULL;
    wchar_t docType[] = L"RAW";
    docInfo.pDatatype = docType;

    if (StartDocPrinterW(hPrinter, 1, reinterpret_cast<LPBYTE>(&docInfo)) == 0) {
        ClosePrinter(hPrinter);
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::PrinterUnavailable, "Failed to start document on printer.")
        );
    }

    StartPagePrinter(hPrinter);

    DWORD written = 0;
    BOOL bSuccess = WritePrinter(hPrinter, const_cast<char*>(data.constData()), static_cast<DWORD>(data.size()), &written);

    EndPagePrinter(hPrinter);
    EndDocPrinter(hPrinter);
    ClosePrinter(hPrinter);

    if (!bSuccess) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::PrinterUnavailable, "WritePrinter failed.")
        );
    }
    return core::Result<void, core::AppError>::ok();
#else
    return core::Result<void, core::AppError>::ok();
#endif
}

} // namespace printing
