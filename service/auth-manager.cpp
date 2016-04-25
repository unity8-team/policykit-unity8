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

#include "auth-manager.h"
#include "authentication.h"

#include <libnotify/notify.h>

AuthManager::AuthManager()
{
    auto success = thread.executeOnThread<bool>([]() {
        /* Initialize Libnotify */
        auto initsuccess = notify_init("unity8-policy-kit");

        if (initsuccess == FALSE)
            throw std::runtime_error("Unable to initalize libnotify");

        /* Ensure the server has what we need */
        auto caps = notify_get_server_caps();
        bool hasDialogs = false;
        for (auto cap = caps; cap != nullptr && !hasDialogs; cap = g_list_next(cap))
        {
            auto capname = static_cast<const gchar *>(cap->data);
            if (capname == nullptr)
                continue;

            if (std::string(capname) == "x-canonical-private-synchronous")
                hasDialogs = true;
        }
        g_list_free_full(caps, g_free);

        if (!hasDialogs)
        {
            notify_uninit();
            throw std::runtime_error("Notification server doesn't have the capability to show dialogs!");
        }

        return true;
    });

    if (success)
        g_debug("Authentication Manager initialized");
}

AuthManager::~AuthManager()
{
    thread.executeOnThread<bool>([this]() {
        /* Cancel our authentications */
        while (!in_flight.empty())
        {
            /* Use a while because we modify the list internally */
            cancelAuthentication((*in_flight.begin()).first);
            thread.runQueuedJobs();
        }

        /* Uninitialize libnotify for the system */
        notify_uninit();
        return true;
    });
}

/** \brief Starts an Authentication
        \param action_id Type of action from PolicyKit
        \param message Message to show to the user
        \param icon_name Icon to show with the notification
        \param cookie Unique string to track the authentication
        \param identities Identities that can be used to authenticate this action
        \param finishedCallback Function to call when the user has completed the authorization

        Creates the authentication object on the notification thread
        using the buildAuthentication function. It also creates a more
        complex callback where, when the callback is called it also removes
        this authentication from the in_flight map which is tracking
        Authentication objects.
*/
std::string AuthManager::createAuthentication(const std::string &action_id,
                                              const std::string &message,
                                              const std::string &icon_name,
                                              const std::string &cookie,
                                              const std::list<std::string> &identities,
                                              const std::function<void(Authentication::State)> &finishedCallback)
{
    return thread.executeOnThread<std::string>(
        [this, &action_id, &message, &icon_name, &cookie, &identities, &finishedCallback]() {
            /* Build the authentication object */
            auto auth = buildAuthentication(
                action_id, message, icon_name, cookie, identities,
                [this, cookie, finishedCallback](Authentication::State state) {
                    this->thread.timeout(
                        std::chrono::hours{0},
                        [this, cookie]() {
                            auto entry = in_flight.find(cookie);

                            if (entry == in_flight.end())
                            {
                                throw std::runtime_error("Handle for Authentication '" + cookie +
                                                         "' isn't found in 'in_flight' authentication map");
                            }

                            in_flight.erase(entry);
                        });

                    /* Up the chain */
                    finishedCallback(state);
                });

            /* Throw it in our queue */
            in_flight[cookie] = auth;

            auth->start();

            return cookie;
        });
}

/** The actual call to create the object, split out so that it can
    be replaced in the test suite with a mock. */
std::shared_ptr<Authentication> AuthManager::buildAuthentication(
    const std::string &action_id,
    const std::string &message,
    const std::string &icon_name,
    const std::string &cookie,
    const std::list<std::string> &identities,
    const std::function<void(Authentication::State)> &finishedCallback)
{
    return std::make_shared<Authentication>(

        action_id, message, icon_name, cookie, identities, finishedCallback);
}

/** Cancels an Authentication that is currently running.
    \param handle the handle of the Authentication object
*/
bool AuthManager::cancelAuthentication(const std::string &handle)
{
    return thread.executeOnThread<bool>([this, &handle]() {
        auto entry = in_flight.find(handle);
        if (entry == in_flight.end())
        {
            g_debug("Unable to find authentication '%s' to cancel", handle.c_str());
            return false;
        }

        /* This should change the state which will cause it to be
           dropped from the in_flight map */
        (*entry).second->cancel();

        return true;
    });
}
