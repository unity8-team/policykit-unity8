
#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>

#include "authentication.h"
#include "glib-thread.h"

class AuthManager
{
public:
    AuthManager();
    virtual ~AuthManager();

    virtual std::string createAuthentication(const std::string& action_id,
                                             const std::string& message,
                                             const std::string& icon_name,
                                             const std::string& cookie,
                                             const std::list<std::string>& identities,
                                             const std::function<void(Authentication::State)>& finishedCallback);
    virtual bool cancelAuthentication(const std::string& handle);

private:
    std::map<std::string, std::shared_ptr<Authentication>> inFlight;
    GLib::ContextThread thread;
};
