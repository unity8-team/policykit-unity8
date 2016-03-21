/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Ted Gould <ted@canonical.com>
 */

#include <algorithm>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <type_traits>

#include <gio/gio.h>

#include <libdbustest/dbus-test.h>

#include "glib-thread.h"

class PolicyKitMock
{
    DbusTestDbusMock *mock = nullptr;
    DbusTestDbusMockObject *baseobj = nullptr;
    GLib::ContextThread thread;

public:
    PolicyKitMock()
    {
        mock = dbus_test_dbus_mock_new("org.freedesktop.PolicyKit1");
        dbus_test_task_set_bus(DBUS_TEST_TASK(mock), DBUS_TEST_SERVICE_BUS_SYSTEM);
        dbus_test_task_set_name(DBUS_TEST_TASK(mock), "Policy");

        baseobj = dbus_test_dbus_mock_get_object(mock, "/org/freedesktop/PolicyKit1/Authority",
                                                 "org.freedesktop.PolicyKit1.Authority", nullptr);

        dbus_test_dbus_mock_object_add_method(mock, baseobj, "RegisterAuthenticationAgent",
                                              G_VARIANT_TYPE("((sa{sv})ss)"), nullptr, "", nullptr);

        dbus_test_dbus_mock_object_add_method(mock, baseobj, "UnregisterAuthenticationAgent",
                                              G_VARIANT_TYPE("((sa{sv})s)"), nullptr, "", nullptr);

        dbus_test_dbus_mock_object_add_method(mock, baseobj, "AuthenticationAgentResponse2",
                                              G_VARIANT_TYPE("(us(sa{sv}))"), nullptr, "", nullptr);
    }

    ~PolicyKitMock()
    {
        g_debug("Destroying the PolicyKit Mock");
        g_clear_object(&mock);
    }

    operator std::shared_ptr<DbusTestTask>()
    {
        std::shared_ptr<DbusTestTask> retval(DBUS_TEST_TASK(g_object_ref(mock)),
                                             [](DbusTestTask *task) { g_clear_object(&task); });
        return retval;
    }

    operator DbusTestTask *()
    {
        return DBUS_TEST_TASK(mock);
    }

    operator DbusTestDbusMock *()
    {
        return mock;
    }

    std::list<std::pair<std::string, std::map<std::string, std::shared_ptr<GVariant>>>> userIdentity()
    {
        return {
            {"unix-user",
             {{"uid", std::shared_ptr<GVariant>(g_variant_ref_sink(g_variant_new_uint32(getuid())), g_variant_unref)}}},
        };
    }

    std::future<bool> beginAuthentication(
        const std::string &dbusAddress,
        const std::string &dbusPath,
        const std::string &action_id,
        const std::string &message,
        const std::string &icon_name,
        const std::list<std::pair<std::string, std::string>> &details,
        const std::string &cookie,
        const std::list<std::pair<std::string, std::map<std::string, std::shared_ptr<GVariant>>>> &identities)
    {

        return thread.executeOnThread<std::future<bool>>(
            [dbusAddress, dbusPath, action_id, message, icon_name, details, cookie, identities]() {
                std::promise<bool> *promise = new std::promise<bool>();

                GVariantBuilder builder;
                g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
                g_variant_builder_add_value(&builder, g_variant_new_string(action_id.c_str()));
                g_variant_builder_add_value(&builder, g_variant_new_string(message.c_str()));
                g_variant_builder_add_value(&builder, g_variant_new_string(icon_name.c_str()));

                g_variant_builder_open(&builder, G_VARIANT_TYPE("a{ss}"));
                for (auto detail : details)
                {
                    g_variant_builder_open(&builder, G_VARIANT_TYPE("{ss}"));
                    g_variant_builder_add_value(&builder, g_variant_new_string(detail.first.c_str()));
                    g_variant_builder_add_value(&builder, g_variant_new_string(detail.second.c_str()));
                    g_variant_builder_close(&builder);
                }
                g_variant_builder_close(&builder);

                g_variant_builder_add_value(&builder, g_variant_new_string(cookie.c_str()));

                g_variant_builder_open(&builder, G_VARIANT_TYPE("a(sa{sv})"));
                for (auto identity : identities)
                {
                    g_variant_builder_open(&builder, G_VARIANT_TYPE("(sa{sv})"));
                    g_variant_builder_add_value(&builder, g_variant_new_string(identity.first.c_str()));
                    g_variant_builder_open(&builder, G_VARIANT_TYPE("a{sv}"));
                    for (auto identdetail : identity.second)
                    {
                        g_variant_builder_open(&builder, G_VARIANT_TYPE("{sv}"));
                        g_variant_builder_add_value(&builder, g_variant_new_string(identdetail.first.c_str()));
                        g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
                        g_variant_builder_add_value(&builder, identdetail.second.get());
                        g_variant_builder_close(&builder);
                        g_variant_builder_close(&builder);
                    }
                    g_variant_builder_close(&builder);
                    g_variant_builder_close(&builder);
                }
                g_variant_builder_close(&builder);

                auto system = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
                if (system == nullptr)
                {
                    promise->set_value(false);

                    auto var = g_variant_builder_end(&builder);
                    g_variant_ref_sink(var);
                    g_variant_unref(var);
                }
                else
                {

                    g_dbus_connection_call(system, dbusAddress.c_str(), dbusPath.c_str(),
                                           "org.freedesktop.PolicyKit1.AuthenticationAgent", "BeginAuthentication",
                                           g_variant_builder_end(&builder), nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                           -1,      /* default timeout */
                                           nullptr, /* cancellable */
                                           dbusMessageCallback, promise);

                    g_object_unref(system);
                }

                return promise->get_future();
            });
    }

    std::future<bool> cancelAuthentication(const std::string &dbusAddress,
                                           const std::string &dbusPath,
                                           const std::string &cookie)
    {
        return thread.executeOnThread<std::future<bool>>([dbusAddress, dbusPath, cookie]() {
            std::promise<bool> *promise = new std::promise<bool>();

            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
            g_variant_builder_add_value(&builder, g_variant_new_string(cookie.c_str()));

            auto system = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
            if (system == nullptr)
            {
                promise->set_value(false);

                auto var = g_variant_builder_end(&builder);
                g_variant_ref_sink(var);
                g_variant_unref(var);
            }
            else
            {

                g_dbus_connection_call(system, dbusAddress.c_str(), dbusPath.c_str(),
                                       "org.freedesktop.PolicyKit1.AuthenticationAgent", "CancelAuthentication",
                                       g_variant_builder_end(&builder), nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                       -1,      /* default timeout */
                                       nullptr, /* cancellable */
                                       dbusMessageCallback, promise);

                g_object_unref(system);
            }

            return promise->get_future();
        });
    }

    static void dbusMessageCallback(GObject *source_object, GAsyncResult *res, gpointer user_data)
    {
        auto promise = reinterpret_cast<std::promise<bool> *>(user_data);
        GError *error = nullptr;

        auto var = g_dbus_connection_call_finish(reinterpret_cast<GDBusConnection *>(source_object), res, &error);

        if (error != nullptr)
        {
            g_warning("Unable to send dbus message: %s", error->message);
            g_error_free(error);
            promise->set_value(false);
        }
        else
        {
            g_debug("DBus Message complete");
            if (var != nullptr)
                g_variant_unref(var);
            promise->set_value(true);
        }

        delete promise;
    }
};
