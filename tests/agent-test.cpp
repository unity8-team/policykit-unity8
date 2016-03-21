
/* Test Libraries */
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>

/* Mocks */
#include "policykit-mock.h"

/* Local Headers */
#include "agent.h"
#include "auth-manager.h"

/* System Libs */
#include <chrono>
#include <thread>

class AgentTest : public ::testing::Test
{
protected:
    DbusTestService *service = NULL;

    GDBusConnection *system = NULL;
    std::shared_ptr<PolicyKitMock> policykit;

    virtual void SetUp()
    {
        service = dbus_test_service_new(NULL);
        dbus_test_service_set_bus(service, DBUS_TEST_SERVICE_BUS_SYSTEM);

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

        policykit = std::make_shared<PolicyKitMock>();

        dbus_test_service_add_task(service, (DbusTestTask *)*policykit);
        dbus_test_service_start_tasks(service);

        system = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
        ASSERT_NE(nullptr, system);
        g_dbus_connection_set_exit_on_close(system, FALSE);
        g_object_add_weak_pointer(G_OBJECT(system), (gpointer *)&system);
    }

    virtual void TearDown()
    {
        policykit.reset();
        g_clear_object(&service);

        g_object_unref(system);

        unsigned int cleartry = 0;
        while (system != NULL && cleartry < 100)
        {
            loop(100);
            cleartry++;
        }

        ASSERT_EQ(nullptr, system);
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
                 AuthManager::AuthHandle(std::string,
                                         std::string,
                                         std::string,
                                         std::string,
                                         std::list<std::string>,
                                         std::function<void(const Authentication &)>));
    MOCK_METHOD1(cancelAuthentication, void(AuthManager::AuthHandle));
};

class AuthenticationNoErrorFake : public Authentication
{
public:
    AuthenticationNoErrorFake()
    {
    }
    virtual std::string getError() const
    {
        return "";
    }

    virtual AuthManager::AuthHandle getCookie() const
    {
        return "cookie-monster";
    }
};

class AuthenticationCancelFake : public Authentication
{
public:
    AuthenticationCancelFake()
    {
    }

    virtual std::string getError() const
    {
        return "Cancelled";
    }

    virtual AuthManager::AuthHandle getCookie() const
    {
        return "cookie-monster";
    }
};

template <typename AuthT>
class AuthCallbackMatcher
{
public:
    template <typename T>
    bool MatchAndExplain(T callback, ::testing::MatchResultListener *listener) const
    {
        g_debug("Callback for AuthCallbackMatcher");
        auto fake = AuthT();
        callback(fake);
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

template <typename AuthT>
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
                                  auto fake = AuthT();
                                  g_debug("Fake cookie: %s", fake.getCookie().c_str());
                                  (*callback)(fake);
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

inline ::testing::PolymorphicMatcher<AuthCallbackMatcher<AuthenticationNoErrorFake>> AuthNoErrorCallback()
{
    return ::testing::MakePolymorphicMatcher(AuthCallbackMatcher<AuthenticationNoErrorFake>());
}

inline ::testing::PolymorphicMatcher<AuthCallbackDelay<AuthenticationCancelFake>> AuthDelayCancelCallback()
{
    return ::testing::MakePolymorphicMatcher(AuthCallbackDelay<AuthenticationCancelFake>());
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

    EXPECT_CALL(*managermock, cancelAuthentication("cookie-monster")).WillOnce(testing::Return());

    auto cancelfuture = policykit->cancelAuthentication(g_dbus_connection_get_unique_name(system),
                                                        "/com/canonical/unity8/policyKit", "cookie-monster");

    EXPECT_EQ(true, cancelfuture.get());
    EXPECT_EQ(false, beginfuture.get());
}

class AuthManagerCancelFake : public AuthManager
{
public:
    std::map<AuthManager::AuthHandle,
             std::tuple<std::string,
                        std::string,
                        std::string,
                        std::string,
                        std::list<std::string>,
                        std::function<void(const Authentication &)>>>
        openAuths;

    AuthManager::AuthHandle createAuthentication(std::string action_id,
                                                 std::string message,
                                                 std::string icon_name,
                                                 std::string cookie,
                                                 std::list<std::string> identities,
                                                 std::function<void(const Authentication &)> finishedCallback)
    {
        openAuths.emplace(cookie, std::make_tuple(action_id, message, icon_name, cookie, identities, finishedCallback));
        return cookie;
    }

    void cancelAuthentication(AuthHandle handle)
    {
        auto entry = openAuths.find(handle);
        if (entry == openAuths.end())
            return;

        AuthenticationCancelFake auth;
        std::get<5>((*entry).second)(auth);
        openAuths.erase(entry);
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

    agent.reset();

    EXPECT_EQ(false, beginfuture.get());
}
