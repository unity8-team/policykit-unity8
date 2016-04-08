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

/* Test Libraries */
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>

/* Mocks */
#include "notifications-mock.h"

/* Local Headers */
#include "authentication.h"
#include "session-iface.h"

/* System Libs */
#include <chrono>
#include <thread>

class AuthenticationTest : public ::testing::Test
{
protected:
    DbusTestService *session_service = NULL;
    GDBusConnection *session = NULL;

    std::shared_ptr<NotificationsMock> notifications;

    virtual void SetUp()
    {
        session_service = dbus_test_service_new(nullptr);
        dbus_test_service_set_bus(session_service, DBUS_TEST_SERVICE_BUS_SESSION);

/* Useful for debugging test failures, not needed all the time (until it fails) */
#if 0
        auto bustle = std::shared_ptr<DbusTestTask>(
            []() {
                DbusTestTask *bustle = DBUS_TEST_TASK(dbus_test_bustle_new("notifications-test.bustle"));
                dbus_test_task_set_name(bustle, "Bustle");
                dbus_test_task_set_bus(bustle, DBUS_TEST_SERVICE_BUS_SESSION);
                return bustle;
            }(),
            [](DbusTestTask *bustle) { g_clear_object(&bustle); });
        dbus_test_service_add_task(service, bustle.get());
#endif

        /* Session Mocks */
        notifications = std::make_shared<NotificationsMock>();

        dbus_test_service_add_task(session_service, (DbusTestTask *)*notifications);
        dbus_test_service_start_tasks(session_service);

        /* Watch busses */
        session = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        ASSERT_NE(nullptr, session);
        g_dbus_connection_set_exit_on_close(session, FALSE);
        g_object_add_weak_pointer(G_OBJECT(session), (gpointer *)&session);
    }

    virtual void TearDown()
    {
        notifications.reset();

        g_clear_object(&session_service);

        g_object_unref(session);

        unsigned int cleartry = 0;
        while (session != nullptr && cleartry < 100)
        {
            loop(100);
            cleartry++;
        }

        ASSERT_EQ(nullptr, session);
    }

    static gboolean timeout_cb(gpointer user_data)
    {
        GMainLoop *loop = static_cast<GMainLoop *>(user_data);
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE;
    }

    void loop(unsigned int ms)
    {
        GMainLoop *loop = g_main_loop_new(NULL, FALSE);
        g_timeout_add(ms, timeout_cb, loop);
        g_main_loop_run(loop);
        g_main_loop_unref(loop);
    }
};

class SessionMock : public Session
{
public:
    std::string _identity;
    std::string _cookie;

    SessionMock(const std::string &identity, const std::string &cookie)
        : _identity(identity)
        , _cookie(cookie)
    {
    }

    MOCK_METHOD0(initiate, void(void));

    core::Signal<const std::string &, bool> &request()
    {
        return _request;
    }
    core::Signal<const std::string &, bool> _request;

    core::Signal<const std::string &> &info()
    {
        return _info;
    }
    core::Signal<const std::string &> _info;

    core::Signal<const std::string &> &error()
    {
        return _error;
    }
    core::Signal<const std::string &> _error;

    core::Signal<bool> &complete()
    {
        return _complete;
    }
    core::Signal<bool> _complete;

    MOCK_METHOD1(requestResponse, void(const std::string &));
};

class AuthenticationSessionMock : public Authentication
{
public:
    AuthenticationSessionMock(const std::string &action_id,
                              const std::string &message,
                              const std::string &icon_name,
                              const std::string &cookie,
                              const std::list<std::string> &identities,
                              const std::function<void(State)> &finishedCallback)
        : Authentication(action_id, message, icon_name, cookie, identities, finishedCallback)
    {
    }

    std::shared_ptr<Session> buildSession(const std::string &identity, const std::string &cookie)
    {
        return std::make_shared<SessionMock>(identity, cookie);
    }
};

TEST_F(AuthenticationTest, Init)
{
    AuthenticationSessionMock auth("action-id", "message", "icon-name", "everyone-loves-cookies", {"unix-name:me"},
                                   [](Authentication::State state) { return; });
}
