#pragma once
#include "domain/Models.h"
#include <memory>
#include <optional>

namespace app {

class AppContext {
public:
    static AppContext& instance();

    const domain::User& currentUser() const { return m_currentUser; }
    void setCurrentUser(const domain::User& user) { m_currentUser = user; }

    bool isLoggedIn() const { return m_currentUser.id > 0; }
    bool isAdmin() const { return m_currentUser.role == domain::UserRole::Admin; }
    bool isManagerOrAdmin() const { 
        return m_currentUser.role == domain::UserRole::Admin || m_currentUser.role == domain::UserRole::Manager; 
    }

    std::optional<domain::CashSession> activeSession() const { return m_activeSession; }
    void setActiveSession(const std::optional<domain::CashSession>& session) { m_activeSession = session; }

    void logout() {
        m_currentUser = domain::User{};
        m_activeSession = std::nullopt;
    }

private:
    AppContext();
    domain::User m_currentUser;
    std::optional<domain::CashSession> m_activeSession;
};

} // namespace app
