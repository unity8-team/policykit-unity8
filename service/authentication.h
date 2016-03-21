
#include "auth-manager.h"

#pragma once

#include <string>

class Authentication
{
public:
    Authentication();
    virtual ~Authentication();

    virtual std::string getError() const;
    virtual AuthManager::AuthHandle getCookie() const;
};
