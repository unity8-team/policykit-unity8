
#include "agent.h"
#include "agent-glib.h"

#include <utility>

/* Initialize variables, but also register the interface with DBus which
   makes us ready to start getting messages */
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

/* Make sure to unregister the interface and unregister our cancellables */
Agent::~Agent()
{
    g_debug("Destroying PolicyKit Agent");
    _thread.executeOnThread<bool>([this]() {
        while (!cancellables.empty())
        {
            auto handle = cancellables.begin()->first;
            _authmanager->cancelAuthentication(handle);
            unregisterCancellable(handle);
        }

        polkit_agent_listener_unregister(_agentRegistration);

        return true;
    });
}

/* This is where an auth request comes to us from PolicyKit. Here we handle
   the cancellables and get a handle from the auth manager for cancelling the
   authentication. */
void Agent::authRequest(std::string action_id,
                        std::string message,
                        std::string icon_name,
                        std::string cookie,
                        std::list<std::string> identities,
                        std::shared_ptr<GCancellable> cancellable,
                        std::function<void(Authentication::State)> callback)
{
    gulong connecthandle = 0;
    if (cancellable)
    {
        auto pair = new std::pair<Agent *, std::string>(this, cookie);
        connecthandle = g_cancellable_connect(cancellable.get(), G_CALLBACK(cancelStatic), pair, cancelCleanup);
    }

    g_debug("Saving cancellable: %s", cookie.c_str());
    cancellables.emplace(cookie, std::make_pair(cancellable, connecthandle));

    _authmanager->createAuthentication(action_id, message, icon_name, cookie, identities,
                                       [this, cookie, callback](Authentication::State state) {
                                           _thread.executeOnThread<bool>([this, cookie, callback, state]() {
                                               /* When we handle the callback we need to ensure
                                                  that it happens on the same thread that it came
                                                  from, which is this one. */
                                               unregisterCancellable(cookie);
                                               callback(state);
                                               return true;
                                           });
                                       });
}

/* Static function to do the cancel */
void Agent::cancelStatic(GCancellable *cancel, gpointer user_data)
{
    auto pair = reinterpret_cast<std::pair<Agent *, std::string> *>(user_data);
    pair->first->_authmanager->cancelAuthentication(pair->second);
}

/* Static function to clean up the data needed for cancelling */
void Agent::cancelCleanup(gpointer data)
{
    auto pair = reinterpret_cast<std::pair<Agent *, std::string> *>(data);
    delete pair;
}

/* Disconnect from the g_cancellable */
void Agent::unregisterCancellable(std::string handle)
{
    g_debug("Unregistering cancellable authorization: %s", handle.c_str());
    auto cancel = cancellables.find(handle);
    if (cancel == cancellables.end())
        return;
    g_cancellable_disconnect(cancel->second.first.get(), cancel->second.second);
    cancellables.erase(cancel);
}
