#include "app/AppContext.h"

namespace app {

AppContext& AppContext::instance()
{
    static AppContext inst;
    return inst;
}

AppContext::AppContext()
{
    // Default to admin user for convenient single-store setup
    m_currentUser.id = 1;
    m_currentUser.username = "admin";
    m_currentUser.fullName = "Administrator";
    m_currentUser.role = domain::UserRole::Admin;
    m_currentUser.isActive = true;
}

} // namespace app
