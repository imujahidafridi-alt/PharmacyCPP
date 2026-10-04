#pragma once
#include <QApplication>
#include <memory>
#include "ui/MainWindow.h"

namespace app {

class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    ~Application();

    bool initialize();

private:
    std::unique_ptr<ui::MainWindow> m_mainWindow;
};

} // namespace app
