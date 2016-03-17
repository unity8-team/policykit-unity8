
#include "auth-manager.h"

AuthManager::AuthManager()
{
}

AuthManager::~AuthManager()
{
}

AuthManager::AuthHandle AuthManager::createAuthentication(std::string action_id,
                                                          std::string message,
                                                          std::string icon_name,
                                                          std::string cookie,
                                                          std::list<std::string> identities,
                                                          std::function<void(Authentication *)> finishedCallback)
{
    return 0;
}

void AuthManager::cancelAuthentication(AuthHandle handle)
{
}
