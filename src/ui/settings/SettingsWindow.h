#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

namespace ui {

class SettingsWindow : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget* parent = nullptr);

    void loadSettings();

private slots:
    void handleSaveSettings();
    void handleBackupNow();
    void handleRestoreBackup();
    void handleTestConnection();
    void handleInitDatabase();
    void handlePreviewReceipt();
    void handleDbTypeChanged(int index);

private:
    class AppTextInput* m_storeNameEdit;
    class AppTextInput* m_storePhoneEdit;
    class AppTextInput* m_storeAddressEdit;
    class AppTextInput* m_dslEdit;
    class AppTextInput* m_ntnEdit;
    QCheckBox* m_bismillahCheck;
    class AppTextInput* m_receiptFooterEdit;
    
    class AppDropdown* m_dbTypeCombo;
    class AppTextInput* m_dbHostEdit;
    class AppTextInput* m_dbPortEdit;
    class AppTextInput* m_dbNameEdit;
    class AppTextInput* m_dbUserEdit;
    class AppTextInput* m_dbPassEdit;
    QWidget* m_pgFieldsContainer;

    class AppDropdown* m_printerCombo;
    QCheckBox* m_allowNegativeStockCheck;
    QCheckBox* m_fefoCheck;

    QLabel* m_lastBackupLabel;
};

} // namespace ui
