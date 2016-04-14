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

#pragma once

#include "auth-manager.h"
#include "authentication.h"
#include "glib-thread.h"

#include <functional>
#include <list>
#include <memory>
#include <string>

typedef struct _AgentGlib AgentGlib;

/**
        \brief Class that connects to PolicyKit as the agent and
                gets events on when PK wants an auth check.

        Class to bridge the interface for the PolicyKit agent registration
        and signaling. It uses a small helper GLib object which is a subclass
        of libpolicykitagent's PolkitAgentListener class. This is done in
        GObject, and then this class exists to provide C++ structures around
        that class.

        If requested the agent will use the passed in AuthManager instance to
        create authorization UI's to query the user. It maintains the cancellable
        objects from GLib and will request the AuthManager to cancel any UI
        requests if PolicyKit asks.

        \note The Agent Class should be instantiated once, as PolicyKit only
        allows one agent per session. Creating multiple instances will
        result in PolicyKit returning an error.
*/
class Agent
{
public:
    Agent(const std::shared_ptr<AuthManager> &authmanager);
    ~Agent();

    void authRequest(const std::string &action_id,
                     const std::string &message,
                     const std::string &icon_name,
                     const std::string &cookie,
                     const std::list<std::string> &identities,
                     const std::shared_ptr<GCancellable> &cancellable,
                     const std::function<void(Authentication::State)> &callback);

private:
    /** Auth manager used to create authorization UI's */
    std::shared_ptr<AuthManager> _authmanager;
    /** Thread that the agent runs on */
    GLib::ContextThread _thread;

    /** GLib object that interfaces with libpolicykitagent */
    std::shared_ptr<AgentGlib> _glib;
    /** Handle returned by libpolicykit to track our registration */
    gpointer _agentRegistration;

    /** All the cancellable objects we're tracking indexed by the
        cookie that they were associated with. */
    std::map<std::string, std::pair<std::shared_ptr<GCancellable>, gulong>> cancellables;

    void unregisterCancellable(const std::string &handle);

    static void cancelStatic(GCancellable *cancel, gpointer user_data);
    static void cancelCleanup(gpointer data);
};
