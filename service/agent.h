
#pragma once

#include "auth-manager.h"
#include "authentication.h"
#include "glib-thread.h"

#include <functional>
#include <list>
#include <memory>
#include <string>

typedef struct _AgentGlib AgentGlib;

class Agent
{
public:
    Agent(const std::shared_ptr<AuthManager> &authmanager);

    void authRequest(std::string action_id,
                     std::string message,
                     std::string icon_name,
                     std::string cookie,
                     std::list<std::string> identities,
                     std::shared_ptr<GCancellable> cancellable,
                     std::function<void(Authentication *)> callback);

private:
    std::shared_ptr<AuthManager> _authmanager;
    GLib::ContextThread _thread;

    std::shared_ptr<AgentGlib> _glib;
};
