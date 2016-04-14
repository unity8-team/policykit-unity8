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

/** \brief Implementation of the Session class

    This implementation wraps up the PolkitAgentSession object with
    some static functions that get turned into core::signal's. It
    also aligns its lifecycle with the GObject one.
*/
class Session::Impl
{
private:
    /** GObject based session object that we're wrapping */
    PolkitAgentSession *session;
    /** A sentinal to say whether complete has been signaled, if not
        we need to cancel before unref'ing the session. */
    bool sessionComplete;

public:
    Impl(const std::string &identity, const std::string &cookie)
        : session(polkit_agent_session_new(polkit_identity_from_string(identity.c_str(), nullptr), cookie.c_str()))
        , sessionComplete(false)
    {
    }

    ~Impl()
    {
        if (!sessionComplete)
            polkit_agent_session_cancel(session);

        g_signal_handler_disconnect(session, gsig_request);
        g_signal_handler_disconnect(session, gsig_show_info);
        g_signal_handler_disconnect(session, gsig_show_error);
        g_signal_handler_disconnect(session, gsig_completed);

        g_clear_object(&session);
    }

    /** Signal from the session that requests information from the user.
        Includes the text to be shown and whether it is a password or not. */
    core::Signal<const std::string &, bool> request;
    /** Signal from the session that includes info to show to the user */
    core::Signal<const std::string &> info;
    /** Signal from the session that includes an error to show to the user */
    core::Signal<const std::string &> error;
    /** Signal from the session that says the session is complete, a boolean
        for whether it was successful or not. */
    core::Signal<bool> complete;

    gulong gsig_request = 0;    /**< GLib signal handle */
    gulong gsig_show_info = 0;  /**< GLib signal handle */
    gulong gsig_show_error = 0; /**< GLib signal handle */
    gulong gsig_completed = 0;  /**< GLib signal handle */

    /** Sends a response to the Polkit Session.
        \param response Text response from the user */
    void requestResponse(const std::string &response)
    {
        polkit_agent_session_response(session, response.c_str());
    }

private:
    /** Static callback for the request signal. Passed up to the
        request C++ signal. */
    static void requestCb(PolkitAgentSession *session, const gchar *text, gboolean password, gpointer user_data)
    {
        g_debug("PK Session Request: %s", text);
        auto obj = reinterpret_cast<Impl *>(user_data);
        obj->request(text, password == TRUE);
    }

    /** Static callback for the info signal. Passed up to the
        info C++ signal. */
    static void infoCb(PolkitAgentSession *session, const gchar *text, gpointer user_data)
    {
        g_debug("PK Session Info: %s", text);
        auto obj = reinterpret_cast<Impl *>(user_data);
        obj->info(text);
    }

    /** Static callback for the error signal. Passed up to the
        error C++ signal. */
    static void errorCb(PolkitAgentSession *session, const gchar *text, gpointer user_data)
    {
        g_debug("PK Session Error: %s", text);
        auto obj = reinterpret_cast<Impl *>(user_data);
        obj->error(text);
    }

    /** Static callback for the complete signal. Passed up to the
        complete C++ signal. Also sets the session complete flag
        which ensures we don't cancel on destruction. */
    static void completeCb(PolkitAgentSession *session, bool success, gpointer user_data)
    {
        auto obj = reinterpret_cast<Impl *>(user_data);
        obj->sessionComplete = true;
        obj->complete(success == TRUE);
    }

public:
    /** Internal implementation functions don't have to have good names */
    void go()
    {
        gsig_request = g_signal_connect(session, "request", G_CALLBACK(requestCb), this);
        gsig_show_info = g_signal_connect(session, "show-info", G_CALLBACK(infoCb), this);
        gsig_show_error = g_signal_connect(session, "show-error", G_CALLBACK(errorCb), this);
        gsig_completed = g_signal_connect(session, "completed", G_CALLBACK(completeCb), this);

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

/** Starts the session so that signals start flowing */
void Session::initiate()
{
    return impl->go();
}

/** Gets the request signal so that it can be connected to. */
core::Signal<const std::string &, bool> &Session::request()
{
    return impl->request;
}

/** Returns a response from the user to the session.
    \param response Text response
*/
void Session::requestResponse(const std::string &response)
{
    return impl->requestResponse(response);
}

/** Gets the info signal so that it can be connected to. */
core::Signal<const std::string &> &Session::info()
{
    return impl->info;
}

/** Gets the error signal so that it can be connected to. */
core::Signal<const std::string &> &Session::error()
{
    return impl->error;
}

/** Gets the complete signal so that it can be connected to. */
core::Signal<bool> &Session::complete()
{
    return impl->complete;
}
