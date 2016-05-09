/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Ted Gould <ted.gould@canonical.com>
 */

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

        /* Setup registration options */
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
		/* Makes us the fallback agent so that system settings can override
		   when it is setting multiple password settings all at once. */
        g_variant_builder_add_parsed(&builder, "{'fallback', true}");

        /* Register it */
        GError *registererror = nullptr;
        gpointer registration_handle = polkit_agent_listener_register_with_options(
            reinterpret_cast<PolkitAgentListener *>(glibagent.get()), POLKIT_AGENT_REGISTER_FLAGS_NONE, subject.get(),
            "/com/canonical/unity8/policyKit", g_variant_builder_end(&builder), _thread.getCancellable().get(),
            &registererror);
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

/** This is where an auth request comes to us from PolicyKit. Here we handle
        the cancellables and get a handle from the auth manager for cancelling the
        authentication.

        \param action_id Type of action from PolicyKit
        \param message Message to show to the user
        \param icon_name Icon to show with the notification
        \param cookie Unique string to track the authentication
        \param identities Identities that can be used to authenticate this action
        \param cancellable Object to notify when we need to cancel the authentication
        \param callback Function to call when the user has completed the authorization
*/
void Agent::authRequest(const std::string &action_id,
                        const std::string &message,
                        const std::string &icon_name,
                        const std::string &cookie,
                        const std::list<std::string> &identities,
                        const std::shared_ptr<GCancellable> &cancellable,
                        const std::function<void(Authentication::State)> &callback)
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

/** Static function to do the cancel */
void Agent::cancelStatic(GCancellable *cancel, gpointer user_data)
{
    auto pair = static_cast<std::pair<Agent *, std::string> *>(user_data);
    pair->first->_authmanager->cancelAuthentication(pair->second);
}

/** Static function to clean up the data needed for cancelling */
void Agent::cancelCleanup(gpointer data)
{
    auto pair = static_cast<std::pair<Agent *, std::string> *>(data);
    delete pair;
}

/** Disconnect from the g_cancellable */
void Agent::unregisterCancellable(const std::string &handle)
{
    g_debug("Unregistering cancellable authorization: %s", handle.c_str());
    auto cancel = cancellables.find(handle);
    if (cancel == cancellables.end())
        return;
    g_cancellable_disconnect(cancel->second.first.get(), cancel->second.second);
    cancellables.erase(cancel);
}
