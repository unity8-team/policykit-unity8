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

#include <functional>
#include <list>
#include <memory>
#include <string>

#include <gio/gio.h>
#include <libnotify/notify.h>

#include "session-iface.h"

class Authentication
{
public:
    /** When the Authentication is complete the result of it. */
    enum class State
    {
        CANCELLED, /**< Authentication was cancelled */
        SUCCESS    /**< Authentication succeeded */
    };

    Authentication(const std::string &action_id,
                   const std::string &message,
                   const std::string &icon_name,
                   const std::string &cookie,
                   const std::list<std::string> &identities,
                   const std::function<void(State)> &finishedCallback);
    virtual ~Authentication();

    virtual void start();
    virtual void cancel();
    virtual void checkResponse();

    /* Update Functions */
    virtual void setInfo(const std::string &info);
    virtual void setError(const std::string &error);
    virtual void addRequest(const std::string &request, bool password);

protected:
    /* Build Functions */
    virtual std::shared_ptr<NotifyNotification> buildNotification(void);
    virtual std::shared_ptr<Session> buildSession(const std::string &identity, const std::string &cookie);

    /* Notification Control */
    virtual void showNotification();
    virtual void hideNotification();

    /* Fini */
    virtual void issueCallback(State state);

private:
    /* Passed in parameters */
    std::string _action_id;                       /**< Type of action from PolicyKit */
    std::string _message;                         /**< Message to show to the user */
    std::string _icon_name;                       /**< Icon to show with the notification */
    std::string _cookie;                          /**< Unique string to track the authentication */
    std::list<std::string> _identities;           /**< Identities that can be used to authenticate this action */
    std::function<void(State)> _finishedCallback; /**< Function to call when the user has completed the authorization */

    /* Internal State */
    bool _callbackSent = false; /**< Ensure that we only call the callback once. */
    guint _actionsExport = 0;   /**< ID returned by GDBus for the export of the action group */
    guint _menusExport = 0;     /**< ID returned by GDBus for the export of the menus */

    /* Stuff we build */
    std::string dbusPath; /**< Unique path we built for this authentication object for exporting things on DBus */
    std::shared_ptr<GDBusConnection>
        _sessionBus; /**< Reference to the session bus so we can ensure it lives as long as we do */
    std::shared_ptr<NotifyNotification> _notification; /**< If we have a notification shown, this is the reference to
                                                          it. May be nullptr. */
    std::shared_ptr<GSimpleActionGroup> _actions;      /**< Action group containing the response action */
    std::shared_ptr<GMenu> _menus; /**< The menu model to export to the snap decision. May include info or error items
                                      as well as the response item. */

    std::shared_ptr<Session> _session; /**< The PolicyKit session that asks us for information */

protected:
    /** Null constructor for mocking in the test suite */
    Authentication()
    {
    }
};
