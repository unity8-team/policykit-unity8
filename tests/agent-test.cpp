
/* Test Libraries */
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>

/* Mocks */
#include "policykit-mock.h"

/* Local Headers */
#include "agent.h"
#include "auth-manager.h"

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
    MOCK_METHOD1(cancelAuthentication, void(AuthManager::AuthHandle));
    // MOCK_METHOD1(cancelAuthentication, decltype(AuthManager->cancelAuthentication));
};

TEST_F(AgentTest, Init)
{
    auto managermock = std::make_shared<AuthManagerMock>();

    Agent agent(managermock);
}
