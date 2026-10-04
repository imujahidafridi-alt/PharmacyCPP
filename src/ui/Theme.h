#pragma once
#include <QString>
#include <QColor>

namespace ui {

class Theme {
public:
    static constexpr const char* PrimaryColor = "#0F766E";       // Deep Emerald
    static constexpr const char* PrimaryHover = "#115E59";
    static constexpr const char* PrimaryActive = "#134E4A";
    
    static constexpr const char* SecondaryColor = "#475569";     // Slate
    static constexpr const char* SecondaryHover = "#334155";
    
    static constexpr const char* BackgroundColor = "#F8FAFC";    // Light neutral
    static constexpr const char* SurfaceColor = "#FFFFFF";       // Pure white cards
    static constexpr const char* BorderColor = "#CBD5E1";        // Subtle slate border
    
    static constexpr const char* TextPrimary = "#0F172A";        // Dark slate text
    static constexpr const char* TextSecondary = "#64748B";      // Muted slate
    
    static constexpr const char* SuccessColor = "#16A34A";       // Fresh green
    static constexpr const char* WarningColor = "#D97706";       // Warm amber
    static constexpr const char* DangerColor = "#DC2626";        // Crisp red
    static constexpr const char* InfoColor = "#2563EB";          // Professional blue

    static QString getAppStyleSheet();
};

} // namespace ui
