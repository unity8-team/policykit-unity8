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
        for (auto cap = caps; cap != nullptr; cap = g_list_next(cap))
        {
            auto capname = reinterpret_cast<const gchar *>(cap->data);

            if (std::string(capname) == "x-canonical-private-synchronous")  // TODO: Lookup name
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
        while (!inFlight.empty())
        {
            /* Use a while because we modify the list internally */
            cancelAuthentication((*inFlight.begin()).first);
            thread.runQueuedJobs();
        }

        /* Uninitialize libnotify for the system */
        notify_uninit();
        return true;
    });
}

std::string AuthManager::createAuthentication(const std::string &action_id,
                                              const std::string &message,
                                              const std::string &icon_name,
                                              const std::string &cookie,
                                              const std::list<std::string> &identities,
                                              const std::function<void(Authentication::State)> &finishedCallback)
{
    return thread.executeOnThread<std::string>([this, &action_id, &message, &icon_name, &cookie, &identities,
                                                &finishedCallback]() {
        /* Build the authentication object */
        auto auth = buildAuthentication(
            action_id, message, icon_name, cookie, identities,
            [this, cookie, finishedCallback](Authentication::State state) {
                this->thread.timeout(std::chrono::hours{0},
                                     [this, cookie]() {
                                         auto entry = inFlight.find(cookie);

                                         if (entry == inFlight.end())
                                         {
                                             throw std::runtime_error("Handle for Authentication '" + cookie +
                                                                      "' isn't found in 'inFlight' authentication map");
                                         }

                                         inFlight.erase(entry);
                                     });

                /* Up the chain */
                finishedCallback(state);
            });

        /* Throw it in our queue */
        inFlight[cookie] = auth;

        auth->start();

        return cookie;
    });
}

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

bool AuthManager::cancelAuthentication(const std::string &handle)
{
    return thread.executeOnThread<bool>([this, &handle]() {
        auto entry = inFlight.find(handle);
        if (entry == inFlight.end())
        {
            g_debug("Unable to find authentication '%s' to cancel", handle.c_str());
            return false;
        }

        /* This should change the state which will cause it to be
           dropped from the inFlight map */
        (*entry).second->cancel();

        return true;
    });
}
