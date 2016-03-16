
#include "agent.h"
#include "agent-glib.h"

Agent::Agent(const std::shared_ptr<AuthManager> &authmanager)
    : _authmanager(authmanager)
    , _thread()
{
    _glib = _thread.executeOnThread<std::shared_ptr<AgentGlib>>([this]() {
        return std::shared_ptr<AgentGlib>(agent_glib_new(this), [](AgentGlib *ptr) { g_clear_object(&ptr); });
    });
}

void Agent::authRequest(std::string action_id,
                        std::string message,
                        std::string icon_name,
                        std::string cookie,
                        std::list<std::string> identities,
                        std::shared_ptr<GCancellable> cancellable,
                        std::function<void(Authentication *)> callback)
{
}
