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

#include "session-iface.h"

#include <polkitagent/polkitagent.h>

class Session::Impl
{
public:
    PolkitAgentSession *session;
    bool sessionComplete;

    Impl(const std::string &identity, const std::string &cookie)
        : session(polkit_agent_session_new(polkit_identity_from_string(identity.c_str(), nullptr), cookie.c_str()))
        , sessionComplete(false)
    {
    }

    ~Impl()
    {
        if (!sessionComplete)
            polkit_agent_session_cancel(session);
        g_clear_object(&session);
    }

    core::Signal<const std::string &, bool> request;
    core::Signal<const std::string &> info;
    core::Signal<const std::string &> error;
    core::Signal<bool> complete;

    void requestResponse(const std::string &response)
    {
        polkit_agent_session_response(session, response.c_str());
    }

    static void requestCb(PolkitAgentSession *session, const gchar *text, gboolean password, gpointer user_data)
    {
        auto obj = reinterpret_cast<Impl *>(user_data);
        obj->request(text, password == TRUE);
    }

    static void infoCb(PolkitAgentSession *session, const gchar *text, gpointer user_data)
    {
        auto obj = reinterpret_cast<Impl *>(user_data);
        obj->info(text);
    }

    static void errorCb(PolkitAgentSession *session, const gchar *text, gpointer user_data)
    {
        auto obj = reinterpret_cast<Impl *>(user_data);
        obj->error(text);
    }

    static void completeCb(PolkitAgentSession *session, bool success, gpointer user_data)
    {
        auto obj = reinterpret_cast<Impl *>(user_data);
        obj->sessionComplete = true;
        obj->complete(success == TRUE);
    }

    void go()
    {
        g_signal_connect(G_OBJECT(session), "request", G_CALLBACK(requestCb), this);
        g_signal_connect(G_OBJECT(session), "show-info", G_CALLBACK(infoCb), this);
        g_signal_connect(G_OBJECT(session), "show-error", G_CALLBACK(errorCb), this);
        g_signal_connect(G_OBJECT(session), "completed", G_CALLBACK(completeCb), this);

        polkit_agent_session_initiate(session);
    }
};

Session::Session(const std::string &identity, const std::string &cookie)
    : impl(std::make_shared<Impl>(identity, cookie))
{
}

Session::~Session()
{
}

void Session::initiate()
{
    return impl->go();
}

core::Signal<const std::string &, bool> &Session::request()
{
    return impl->request;
}

void Session::requestResponse(const std::string &response)
{
    return impl->requestResponse(response);
}

core::Signal<const std::string &> &Session::info()
{
    return impl->info;
}

core::Signal<const std::string &> &Session::error()
{
    return impl->error;
}

core::Signal<bool> &Session::complete()
{
    return impl->complete;
}
