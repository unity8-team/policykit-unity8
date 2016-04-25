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
#include <libnotify/notify.h>
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

        /* Normally done by Auth Manager */
        notify_init("authentication-test");
    }

    virtual void TearDown()
    {
        /* Normally done by Auth Manager */
        notify_uninit();

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
    bool _initiated;

    SessionMock(const std::string &identity)
        : _identity(identity)
        , _initiated(false)
    {
    }

    void initiate() override
    {
        _initiated = true;
    }

    core::Signal<const std::string &, bool> &request() override
    {
        return _request;
    }
    core::Signal<const std::string &, bool> _request;

    core::Signal<const std::string &> &info() override
    {
        return _info;
    }
    core::Signal<const std::string &> _info;

    core::Signal<const std::string &> &error() override
    {
        return _error;
    }
    core::Signal<const std::string &> _error;

    core::Signal<bool> &complete() override
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
        g_debug("Building Authentication object with Session Mock");
    }

    std::weak_ptr<SessionMock> lastSession;

protected:
    std::shared_ptr<Session> buildSession(const std::string &identity) override
    {
        g_debug("Building a Mock session: %s", identity.c_str());
        auto session = std::make_shared<SessionMock>(identity);
        lastSession = session;
        return session;
    }
};

TEST_F(AuthenticationTest, Init)
{
    AuthenticationSessionMock auth("action-id", "message", "icon-name", "everyone-loves-cookies", {"unix-name:me"},
                                   [](Authentication::State state) { return; });
    auth.start();

    ASSERT_FALSE(auth.lastSession.expired());
    EXPECT_EQ("unix-name:me", auth.lastSession.lock()->_identity);
}

TEST_F(AuthenticationTest, Cancel)
{
    Authentication::State cbState;
    bool cbCalled = false;

    AuthenticationSessionMock auth("action-id", "message", "icon-name", "everyone-loves-cookies", {"unix-name:me"},
                                   [&cbState, &cbCalled](Authentication::State state) {
                                       cbState = state;
                                       cbCalled = true;
                                   });

    auth.cancel();
    loop(10);

    EXPECT_TRUE(cbCalled);
    EXPECT_EQ(Authentication::State::CANCELLED, cbState);
}

TEST_F(AuthenticationTest, BasicRequest)
{
    AuthenticationSessionMock auth("action-id", "message", "icon-name", "everyone-loves-cookies", {"unix-name:me"},
                                   [](Authentication::State state) {});
    auth.start();

    auth.setInfo("some info");
    auth.addRequest("password:", true);

    auto dialogs = notifications->getNotifications();

    EXPECT_EQ(1, dialogs.size());

    /* Base Notification */
    EXPECT_EQ("authentication-test", dialogs[0].app_name);
    EXPECT_EQ("icon-name", dialogs[0].app_icon);
    EXPECT_EQ("message", dialogs[0].body);
    EXPECT_EQ(0, dialogs[0].timeout);

    /* Actions */
    EXPECT_EQ(4, dialogs[0].actions.size());
    EXPECT_EQ("okay", dialogs[0].actions[0]);
    EXPECT_EQ("Login", dialogs[0].actions[1]);
    EXPECT_EQ("cancel", dialogs[0].actions[2]);
    EXPECT_EQ("Cancel", dialogs[0].actions[3]);

    /* Hints */
    EXPECT_EQ(2, dialogs[0].hints.size());
    EXPECT_NE(dialogs[0].hints.end(), dialogs[0].hints.find("x-canonical-snap-decisions"));
    EXPECT_NE(dialogs[0].hints.end(), dialogs[0].hints.find("x-canonical-private-menu-model"));

    /* Setup callback */
    ASSERT_FALSE(auth.lastSession.expired());
    EXPECT_CALL(*(auth.lastSession.lock()), requestResponse("")).WillOnce(testing::Return());

    notifications->emitAction("okay");
    loop(50);
}
