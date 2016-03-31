
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
    enum class State
    {
        INITIALIZED,
        CANCELLED,
        SUCCESS
    };

    Authentication(const std::string &action_id,
                   const std::string &message,
                   const std::string &icon_name,
                   const std::string &cookie,
                   const std::list<std::string> &identities,
                   const std::function<void(State)> &finishedCallback);
    virtual ~Authentication();

    virtual void cancel();
    virtual void checkResponse();

protected:
    /* Build Functions */
    virtual std::shared_ptr<NotifyNotification> buildNotification(void);
    virtual std::shared_ptr<Session> buildSession(const std::string &identity, const std::string &cookie);

    /* Update Functions */
    virtual void setInfo(const std::string &info);
    virtual void setError(const std::string &error);
    virtual void addRequest(const std::string &request, bool password);

    /* Notification Control */
    virtual void showNotification();
    virtual void hideNotification();

    /* Fini */
    virtual void issueCallback(State state);

private:
    /* Passed in parameters */
    std::string _action_id;
    std::string _message;
    std::string _icon_name;
    std::string _cookie;
    std::list<std::string> _identities;
    std::function<void(State)> _finishedCallback;

    /* Internal State */
    bool _callbackSent;
    guint _actionsExport;
    guint _menusExport;

    /* Stuff we build */
    std::string dbusPath;
    std::shared_ptr<GDBusConnection> _sessionBus;
    std::shared_ptr<NotifyNotification> _notification;
    std::shared_ptr<GSimpleActionGroup> _actions;
    std::shared_ptr<GMenu> _menus;

    std::shared_ptr<Session> _session;
};
