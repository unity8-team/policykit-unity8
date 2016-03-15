
#include "polkit-iface.h"

PolkitIface::PolkitIface(const std::shared_ptr<AuthManager> &authmanager)
    : _authmanager(authmanager)
{
}
