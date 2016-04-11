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
#include "agent.h"
#include "auth-manager.h"
#include "authentication.h"

class AuthManagerTest : public ::testing::Test
{
protected:
    DbusTestService* session_service = NULL;
    GDBusConnection* session = NULL;

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

        dbus_test_service_add_task(session_service, (DbusTestTask*)*notifications);
        dbus_test_service_start_tasks(session_service);

        /* Watch busses */
        session = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        ASSERT_NE(nullptr, session);
        g_dbus_connection_set_exit_on_close(session, FALSE);
        g_object_add_weak_pointer(G_OBJECT(session), (gpointer*)&session);
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
        GMainLoop* loop = static_cast<GMainLoop*>(user_data);
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE;
    }

    void loop(unsigned int ms)
    {
        GMainLoop* loop = g_main_loop_new(NULL, FALSE);
        g_timeout_add(ms, timeout_cb, loop);
        g_main_loop_run(loop);
        g_main_loop_unref(loop);
    }
};

class AuthenticationMock : public Authentication
{
public:
    AuthenticationMock(const std::string& action_id,
                       const std::string& message,
                       const std::string& icon_name,
                       const std::string& cookie,
                       const std::list<std::string>& identities,
                       const std::function<void(State)>& finishedCallback)
        : Authentication()
        , _action_id(action_id)
        , _message(message)
        , _icon_name(icon_name)
        , _cookie(cookie)
        , _identities(identities)
        , _finishedCallback(finishedCallback)
    {
    }

    virtual void cancel() override
    {
        g_debug("Mock cancelled");
        _finishedCallback(Authentication::State::CANCELLED);
    }

    std::string _action_id;
    std::string _message;
    std::string _icon_name;
    std::string _cookie;
    std::list<std::string> _identities;
    std::function<void(State)> _finishedCallback;
};

class AuthManagerAuthMock : public AuthManager
{
public:
    AuthManagerAuthMock()
        : AuthManager()
    {
    }

    virtual ~AuthManagerAuthMock()
    {
    }

    std::weak_ptr<AuthenticationMock> lastMock;

    std::shared_ptr<Authentication> buildAuthentication(
        const std::string& action_id,
        const std::string& message,
        const std::string& icon_name,
        const std::string& cookie,
        const std::list<std::string>& identities,
        const std::function<void(Authentication::State)>& finishedCallback) override
    {
        g_debug("Building Mock Authentication");
        auto auth =
            std::make_shared<AuthenticationMock>(action_id, message, icon_name, cookie, identities, finishedCallback);
        lastMock = auth;
        return auth;
    }
};

TEST_F(AuthManagerTest, Init)
{
    AuthManagerAuthMock authman;
}

TEST_F(AuthManagerTest, createAuth)
{
    AuthManagerAuthMock authman;
    bool callback = false;

    authman.createAuthentication("action-id", "message", "icon-name", "everyone-loves-cookies", {"unix-name:me"},
                                 [&callback](Authentication::State state) { callback = true; });

    ASSERT_FALSE(authman.lastMock.expired());
    EXPECT_EQ("action-id", authman.lastMock.lock()->_action_id);
    EXPECT_EQ("message", authman.lastMock.lock()->_message);
    EXPECT_EQ("icon-name", authman.lastMock.lock()->_icon_name);
    EXPECT_EQ("everyone-loves-cookies", authman.lastMock.lock()->_cookie);
    EXPECT_EQ(std::list<std::string>({"unix-name:me"}), authman.lastMock.lock()->_identities);
    EXPECT_TRUE((bool)(authman.lastMock.lock()->_finishedCallback));

    authman.lastMock.lock()->_finishedCallback(Authentication::State::CANCELLED);
    EXPECT_EQ(true, callback);
}

TEST_F(AuthManagerTest, cancelAuth)
{
    AuthManagerAuthMock authman;

    /* Make sure this doesn't fail */
    EXPECT_FALSE(authman.cancelAuthentication("everyone-loves-cookies"));

    bool callback_cancelled = false;
    authman.createAuthentication("action-id", "message", "icon-name", "everyone-loves-cookies", {"unix-name:me"},
                                 [&callback_cancelled](Authentication::State state) {
                                     callback_cancelled = state == Authentication::State::CANCELLED;
                                 });

    EXPECT_TRUE(authman.cancelAuthentication("everyone-loves-cookies"));
    EXPECT_TRUE(callback_cancelled);
}

TEST_F(AuthManagerTest, badServer)
{
    std::shared_ptr<AuthManager> authman;

    ASSERT_NE(nullptr, notifications);

    dbus_test_service_remove_task(session_service, (DbusTestTask*)*notifications);
    notifications.reset();

    loop(10);

    EXPECT_THROW(authman = std::make_shared<AuthManager>(), std::runtime_error);

    notifications = std::make_shared<NotificationsMock>(std::vector<std::string>({"not-a-capability-we-care-about"}));
    dbus_test_service_add_task(session_service, (DbusTestTask*)*notifications);
    dbus_test_task_run((DbusTestTask*)*notifications);

    EXPECT_THROW(authman = std::make_shared<AuthManager>(), std::runtime_error);
}
