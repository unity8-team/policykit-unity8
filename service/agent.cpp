
#include "agent.h"
#include "agent-glib.h"

#include <utility>

Agent::Agent(const std::shared_ptr<AuthManager> &authmanager)
    : _authmanager(authmanager)
    , _thread()
{
    std::tie(_glib,
             _agentRegistration) = _thread.executeOnThread<std::pair<std::shared_ptr<AgentGlib>, gpointer>>([this]() {
        /* Get a session */
        GError *subjecterror = nullptr;
        auto subject = std::shared_ptr<PolkitSubject>(
            polkit_unix_session_new_for_process_sync(getpid(), _thread.getCancellable().get(), &subjecterror),
            [](PolkitSubject *subj) { g_clear_object(&subj); });
        if (subjecterror != nullptr)
        {
            auto memwrapper = std::shared_ptr<GError>(subjecterror, [](GError *error) { g_error_free(error); });
            throw std::runtime_error("Unable to get Unix PID subject: " + std::string(memwrapper.get()->message));
        }

        /* Build our Agent subclass */
        auto glibagent = std::shared_ptr<AgentGlib>(agent_glib_new(this), [](AgentGlib *ptr) { g_clear_object(&ptr); });

        /* Register it */
        GError *registererror = nullptr;
        gpointer registration_handle = polkit_agent_listener_register(
            reinterpret_cast<PolkitAgentListener *>(glibagent.get()), POLKIT_AGENT_REGISTER_FLAGS_NONE, subject.get(),
            "/com/canonical/unity8/policyKit", _thread.getCancellable().get(), &registererror);
        if (registererror != nullptr)
        {
            auto memwrapper = std::shared_ptr<GError>(registererror, [](GError *error) { g_error_free(error); });
            throw std::runtime_error("Unable to register agent: " + std::string(memwrapper.get()->message));
        }

        return std::make_pair(glibagent, registration_handle);
    });
}

Agent::~Agent()
{
    _thread.executeOnThread<bool>([this]() {
        polkit_agent_listener_unregister(_agentRegistration);
        return true;
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
