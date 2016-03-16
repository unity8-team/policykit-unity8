
#include "auth-manager.h"

#pragma once

#include <string>

class Authentication
{
public:
    Authentication();

    std::string getError();
    AuthManager::AuthHandle getHandle();
};
