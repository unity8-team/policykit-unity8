
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
    ~Agent();

    void authRequest(std::string action_id,
                     std::string message,
                     std::string icon_name,
                     std::string cookie,
                     std::list<std::string> identities,
                     std::shared_ptr<GCancellable> cancellable,
                     std::function<void(Authentication::State)> callback);

private:
    std::shared_ptr<AuthManager> _authmanager;
    GLib::ContextThread _thread;

    std::shared_ptr<AgentGlib> _glib;
    gpointer _agentRegistration;

    std::map<std::string, std::pair<std::shared_ptr<GCancellable>, gulong>> cancellables;
    void unregisterCancellable(std::string handle);

    static void cancelStatic(GCancellable *cancel, gpointer user_data);
    static void cancelCleanup(gpointer data);
};
