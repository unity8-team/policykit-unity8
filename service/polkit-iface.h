
#pragma once

#include "auth-manager.h"

#include <memory>

class PolkitIface
{
public:
    PolkitIface(const std::shared_ptr<AuthManager> &authmanager);

private:
    std::shared_ptr<AuthManager> _authmanager;
};
