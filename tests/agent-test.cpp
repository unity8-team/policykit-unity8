
/* Test Libraries */
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>

/* Mocks */
#include "notifications-mock.h"
#include "policykit-mock.h"

/* Local Headers */
#include "agent.h"
#include "auth-manager.h"
#include "authentication.h"

/* System Libs */
#include <chrono>
#include <thread>

class AgentTest : public ::testing::Test
{
protected:
    DbusTestService *system_service = NULL;
    GDBusConnection *system = NULL;

    DbusTestService *session_service = NULL;
    GDBusConnection *session = NULL;

    std::shared_ptr<PolicyKitMock> policykit;
    std::shared_ptr<NotificationsMock> notifications;

    virtual void SetUp()
    {
        system_service = dbus_test_service_new(nullptr);
        dbus_test_service_set_bus(system_service, DBUS_TEST_SERVICE_BUS_SYSTEM);

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

        /* System Mocks */
        policykit = std::make_shared<PolicyKitMock>();

        dbus_test_service_add_task(system_service, (DbusTestTask *)*policykit);
        dbus_test_service_start_tasks(system_service);

        /* Session Mocks */
        notifications = std::make_shared<NotificationsMock>();

        dbus_test_service_add_task(session_service, (DbusTestTask *)*notifications);
        dbus_test_service_start_tasks(session_service);

        /* Watch busses */
        system = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
        ASSERT_NE(nullptr, system);
        g_dbus_connection_set_exit_on_close(system, FALSE);
        g_object_add_weak_pointer(G_OBJECT(system), (gpointer *)&system);

        session = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        ASSERT_NE(nullptr, session);
        g_dbus_connection_set_exit_on_close(session, FALSE);
        g_object_add_weak_pointer(G_OBJECT(session), (gpointer *)&session);
    }

    virtual void TearDown()
    {
        policykit.reset();
        notifications.reset();

        g_clear_object(&system_service);
        g_clear_object(&session_service);

        g_object_unref(system);
        g_object_unref(session);

        unsigned int cleartry = 0;
        while (system != nullptr && session != nullptr && cleartry < 100)
        {
            loop(100);
            cleartry++;
        }

        ASSERT_EQ(nullptr, system);
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

class AuthManagerMock : public AuthManager
{
public:
    MOCK_METHOD6(createAuthentication,
                 std::string(const std::string &,
                             const std::string &,
                             const std::string &,
                             const std::string &,
                             const std::list<std::string> &,
                             const std::function<void(Authentication::State)> &));
    MOCK_METHOD1(cancelAuthentication, bool(const std::string &));
};

class AuthCallbackMatcher
{
    Authentication::State _state;

public:
    AuthCallbackMatcher(Authentication::State state)
        : _state(state)
    {
    }

    template <typename T>
    bool MatchAndExplain(T callback, ::testing::MatchResultListener *listener) const
    {
        g_debug("Callback for AuthCallbackMatcher");
        callback(_state);
        return true;
    }

    void DescribeTo(::std::ostream *os) const
    {
        *os << "is a callback";
    }
    void DescribeNegationTo(::std::ostream *os) const
    {
        *os << "is not a callback";
    }
};

template <Authentication::State state>
class AuthCallbackDelay
{
public:
    template <typename T>
    bool MatchAndExplain(T callback, ::testing::MatchResultListener *listener) const
    {
        auto alloct = new T(callback);
        auto source = g_timeout_source_new(100);
        g_source_set_callback(source,
                              [](gpointer user_data) -> gboolean {
                                  g_debug("Callback for AuthCallbackDelay");
                                  auto callback = reinterpret_cast<T *>(user_data);
                                  (*callback)(state);
                                  return G_SOURCE_REMOVE;
                              },
                              alloct,
                              [](gpointer user_data) -> void {
                                  auto callback = reinterpret_cast<T *>(user_data);
                                  delete callback;
                              });

        auto context = g_main_context_get_thread_default();
        g_source_attach(source, context);

        return true;
    }

    void DescribeTo(::std::ostream *os) const
    {
        *os << "is a callback";
    }
    void DescribeNegationTo(::std::ostream *os) const
    {
        *os << "is not a callback";
    }
};

inline ::testing::PolymorphicMatcher<AuthCallbackMatcher> AuthNoErrorCallback()
{
    return ::testing::MakePolymorphicMatcher(AuthCallbackMatcher(Authentication::State::SUCCESS));
}

inline ::testing::PolymorphicMatcher<AuthCallbackDelay<Authentication::State::CANCELLED>> AuthDelayCancelCallback()
{
    return ::testing::MakePolymorphicMatcher(AuthCallbackDelay<Authentication::State::CANCELLED>());
}

/* Test to make sure we can just build a default object
   and nothing breaks. Good first test. */
TEST_F(AgentTest, Init)
{
    auto managermock = std::make_shared<AuthManagerMock>();

    Agent agent(managermock);
}

/* Checking that we can start an authentication setup
   and get to the mock */
TEST_F(AgentTest, StandardRequest)
{
    auto managermock = std::make_shared<AuthManagerMock>();

    Agent agent(managermock);

    EXPECT_CALL(*managermock, createAuthentication("my-action", "Do an authentication", "icon-name", "cookie-monster",
                                                   testing::_, AuthNoErrorCallback()))
        .WillOnce(testing::Return("cookie-monster"));

    auto beginfuture =
        policykit->beginAuthentication(g_dbus_connection_get_unique_name(system), "/com/canonical/unity8/policyKit",
                                       "my-action", "Do an authentication", "icon-name", {}, /* details */
                                       "cookie-monster", policykit->userIdentity());

    EXPECT_EQ(true, beginfuture.get());
}

TEST_F(AgentTest, CancelRequest)
{
    auto managermock = std::make_shared<AuthManagerMock>();

    Agent agent(managermock);

    EXPECT_CALL(*managermock, createAuthentication("my-action", "Do an authentication", "icon-name", "cookie-monster",
                                                   testing::_, AuthDelayCancelCallback()))
        .WillOnce(testing::Return("cookie-monster"));

    auto beginfuture =
        policykit->beginAuthentication(g_dbus_connection_get_unique_name(system), "/com/canonical/unity8/policyKit",
                                       "my-action", "Do an authentication", "icon-name", {}, /* details */
                                       "cookie-monster", policykit->userIdentity());

    EXPECT_CALL(*managermock, cancelAuthentication("cookie-monster")).WillOnce(testing::Return(true));

    auto cancelfuture = policykit->cancelAuthentication(g_dbus_connection_get_unique_name(system),
                                                        "/com/canonical/unity8/policyKit", "cookie-monster");

    EXPECT_EQ(true, cancelfuture.get());
    EXPECT_EQ(false, beginfuture.get());
}

class AuthManagerCancelFake : public AuthManager
{
public:
    std::map<std::string,
             std::tuple<std::string,
                        std::string,
                        std::string,
                        std::string,
                        std::list<std::string>,
                        std::function<void(Authentication::State)>>>
        openAuths;

    virtual std::string createAuthentication(const std::string &action_id,
                                             const std::string &message,
                                             const std::string &icon_name,
                                             const std::string &cookie,
                                             const std::list<std::string> &identities,
                                             const std::function<void(Authentication::State)> &finishedCallback)
    {
        openAuths.emplace(cookie, std::make_tuple(action_id, message, icon_name, cookie, identities, finishedCallback));
        return cookie;
    }

    virtual bool cancelAuthentication(const std::string &handle)
    {
        g_debug("Cancelling in 'AuthManagerCancelFake' item: %s", handle.c_str());
        auto entry = openAuths.find(handle);
        if (entry == openAuths.end())
            throw std::runtime_error("Unable to find item: " + handle);

        std::get<5>((*entry).second)(Authentication::State::CANCELLED);
        openAuths.erase(entry);

        return true;
    }
};

TEST_F(AgentTest, ShutdownCancel)
{
    auto managermock = std::make_shared<AuthManagerCancelFake>();

    auto agent = std::make_shared<Agent>(managermock);

    auto beginfuture =
        policykit->beginAuthentication(g_dbus_connection_get_unique_name(system), "/com/canonical/unity8/policyKit",
                                       "my-action", "Do an authentication", "icon-name", {}, /* details */
                                       "cookie-monster", policykit->userIdentity());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(1, managermock->openAuths.size());

    agent.reset();

    EXPECT_EQ(false, beginfuture.get());
}
