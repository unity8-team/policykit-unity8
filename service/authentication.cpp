
#include "authentication.h"

Authentication::Authentication()
{
}

Authentication::~Authentication()
{
}

std::string Authentication::getError() const
{
    return "";
}

AuthManager::AuthHandle Authentication::getHandle() const
{
    return 0;
}
