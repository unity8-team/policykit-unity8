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

#include "authentication.h"

#include <iostream>

/* Make it so all our GObjects are easier to work with */
template <typename T>
class shared_gobject : public std::shared_ptr<T>
{
public:
    shared_gobject(T *obj)
        : std::shared_ptr<T>(obj, [](T *obj) { g_clear_object(&obj); })
    {
    }
};

/* Handle errors into exceptions and make sure we free the
   error as well */
inline static void check_error(GError *error, const std::string &message)
{
    if (error == nullptr)
        return;
    auto fullmessage = message + ": " + error->message;
    g_error_free(error);
    throw std::runtime_error(fullmessage);
}

/* Static helpers for C callbacks */
static void notification_closed(NotifyNotification *notification, gpointer user_data)
{
    auto obj = reinterpret_cast<Authentication *>(user_data);
    obj->cancel();
}

static void notification_action_response(NotifyNotification *notification, char *action, gpointer user_data)
{
    auto obj = reinterpret_cast<Authentication *>(user_data);
    obj->checkResponse();
}

static void notification_action_cancel(NotifyNotification *notification, char *action, gpointer user_data)
{
    auto obj = reinterpret_cast<Authentication *>(user_data);
    obj->cancel();
}

/* Initialize everything */
Authentication::Authentication(const std::string &action_id,
                               const std::string &message,
                               const std::string &icon_name,
                               const std::string &cookie,
                               const std::list<std::string> &identities,
                               const std::function<void(State)> &finishedCallback)
    : _action_id(action_id)
    , _message(message)
    , _icon_name(icon_name)
    , _cookie(cookie)
    , _identities(identities)
    , _finishedCallback(finishedCallback)
    , _callbackSent(false)
    , _actionsExport(0)
    , _menusExport(0)
{
    GError *error = nullptr;

    /* Get the Bus */
    _sessionBus = shared_gobject<GDBusConnection>(g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error));
    check_error(error, "Unable to get session bus");

    /* Build a unique path */
    static unsigned int authentication_count = 0;
    auto thiscnt = ++authentication_count;
    dbusPath = "/com/canonical/unity8/policykit/authentication" + std::to_string(thiscnt);
    g_debug("DBus Path: %s", dbusPath.c_str());

    /* Setup Actions */
    _actions = shared_gobject<GSimpleActionGroup>(g_simple_action_group_new());
    auto pwaction =
        shared_gobject<GSimpleAction>(g_simple_action_new_stateful("response", nullptr, g_variant_new_string("")));
    g_action_map_add_action(G_ACTION_MAP(_actions.get()), G_ACTION(pwaction.get()));

    _actionsExport = g_dbus_connection_export_action_group(_sessionBus.get(), dbusPath.c_str(),
                                                           G_ACTION_GROUP(_actions.get()), &error);
    check_error(error, "Unable to export actions");

    /* Setup Menus */
    _menus = shared_gobject<GMenu>(g_menu_new());
    _menusExport =
        g_dbus_connection_export_menu_model(_sessionBus.get(), dbusPath.c_str(), G_MENU_MODEL(_menus.get()), &error);
    check_error(error, "Unable to export menu model");

    _session = buildSession(*identities.begin(), cookie);
}

Authentication::~Authentication()
{
    /* This will cancel if we haven't already sent a
       complete message to the creator */
    cancel();

    g_dbus_connection_unexport_menu_model(_sessionBus.get(), _menusExport);
    g_dbus_connection_unexport_action_group(_sessionBus.get(), _actionsExport);
}

std::shared_ptr<NotifyNotification> Authentication::buildNotification(void)
{
    /* Build our notification */
    auto notification = shared_gobject<NotifyNotification>(
        notify_notification_new("Elevated permissions required", _message.c_str(), _icon_name.c_str()));
    if (!notification)
        throw std::runtime_error("Unable to setup notification object");

    notify_notification_set_timeout(notification.get(), NOTIFY_EXPIRES_NEVER);
    notify_notification_add_action(notification.get(), "okay", "Login", notification_action_response, this,
                                   nullptr); /* free func */
    notify_notification_add_action(notification.get(), "cancel", "Cancel", notification_action_cancel, this,
                                   nullptr); /* free func */

    g_signal_connect(notification.get(), "closed", G_CALLBACK(notification_closed), this);

    /* Set Notification hints */
    notify_notification_set_hint(notification.get(), "x-canonical-snap-decisions", g_variant_new_string("true"));

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE("{sv}"));
    g_variant_builder_add_value(&builder, g_variant_new_string("busName"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
    g_variant_builder_add_value(&builder, g_variant_new_string(g_dbus_connection_get_unique_name(_sessionBus.get())));
    g_variant_builder_close(&builder);
    g_variant_builder_close(&builder);
    g_variant_builder_open(&builder, G_VARIANT_TYPE("{sv}"));
    g_variant_builder_add_value(&builder, g_variant_new_string("menuPath"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
    g_variant_builder_add_value(&builder, g_variant_new_string(dbusPath.c_str()));
    g_variant_builder_close(&builder);
    g_variant_builder_close(&builder);
    g_variant_builder_open(&builder, G_VARIANT_TYPE("{sv}"));
    g_variant_builder_add_value(&builder, g_variant_new_string("actions"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
    g_variant_builder_open(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE("{sv}"));
    g_variant_builder_add_value(&builder, g_variant_new_string("pk"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
    g_variant_builder_add_value(&builder, g_variant_new_string(dbusPath.c_str()));
    g_variant_builder_close(&builder);
    g_variant_builder_close(&builder);
    g_variant_builder_close(&builder);
    g_variant_builder_close(&builder);
    g_variant_builder_close(&builder);

    notify_notification_set_hint(notification.get(), "x-canonical-private-menu-model", g_variant_builder_end(&builder));

    return notification;
}

std::shared_ptr<Session> Authentication::buildSession(const std::string &identity, const std::string &cookie)
{
    g_debug("Building a new PK session");
    auto session = std::make_shared<Session>(identity, cookie);

    session->request().connect([this](const std::string &prompt, bool password) { addRequest(prompt, password); });

    session->info().connect([this](const std::string &info) { setInfo(info); });

    session->error().connect([this](const std::string &error) { setError(error); });

    session->complete().connect([this](bool success) {
        hideNotification();

        if (success)
        {
            issueCallback(Authentication::State::SUCCESS);
        }
        else
        {
            /* If we're not successful we'll try again */
            _session = buildSession(*_identities.begin(), _cookie);
        }
    });

    session->initiate();

    return session;
}

void Authentication::showNotification()
{
    if (!_notification)
        _notification = buildNotification();

    if (!_notification)
        return;

    g_debug("Showing Notification");

    GError *error = nullptr;
    notify_notification_show(_notification.get(), &error);
    check_error(error, "Unable to show notification");
}

void Authentication::hideNotification()
{
    /* Close the notification */
    if (_notification)
        notify_notification_close(_notification.get(), nullptr);
    _notification.reset();

    /* Clear the menu */
    g_menu_remove_all(G_MENU(_menus.get()));

    /* Clear the response */
    auto action = g_action_map_lookup_action(G_ACTION_MAP(_actions.get()), "response"); /* No transfer */
    if (action != nullptr && G_IS_SIMPLE_ACTION(action))
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_string(""));
}

void Authentication::cancel()
{
    g_debug("Notification Cancelled");
    hideNotification();
    issueCallback(Authentication::State::CANCELLED);
}

void Authentication::checkResponse()
{
    /* Get the password */
    auto vresponse = g_action_group_get_action_state(G_ACTION_GROUP(_actions.get()), "response");
    std::string response(g_variant_get_string(vresponse, nullptr));
    g_variant_unref(vresponse);

    g_debug("Notification response: %s", response.c_str());

    hideNotification();

    _session->requestResponse(response);
}

int findMenuItem(std::shared_ptr<GMenu> &menu, const std::string &type, const std::string &value)
{
    int index = -1;
    for (int i = 0; i < g_menu_model_get_n_items(G_MENU_MODEL(menu.get())); i++)
    {
        auto vtext =
            g_menu_model_get_item_attribute_value(G_MENU_MODEL(menu.get()), i, type.c_str(), G_VARIANT_TYPE_STRING);

        if (vtext == nullptr)
            continue;

        auto text = std::string(g_variant_get_string(vtext, nullptr));
        g_variant_unref(vtext);

        if (text == value)
        {
            index = i;
            break;
        }
    }

    return index;
}

void Authentication::setInfo(const std::string &info)
{
    int index = findMenuItem(_menus, "x-canonical-unity8-policy-kit-type", "info");

    std::shared_ptr<GMenuItem> item;

    if (index == -1)
    {
        /* Build it */
        item = shared_gobject<GMenuItem>(g_menu_item_new(info.c_str(), nullptr));
        g_menu_item_set_attribute_value(item.get(), "x-canonical-unity8-policy-kit-type", g_variant_new_string("info"));
    }
    else
    {
        /* Update it */
        item = shared_gobject<GMenuItem>(g_menu_item_new_from_model(G_MENU_MODEL(_menus.get()), index));
        g_menu_item_set_label(item.get(), info.c_str());
        g_menu_remove(_menus.get(), index);
    }

    g_menu_prepend_item(_menus.get(), item.get());
}

void Authentication::setError(const std::string &error)
{
    int index = findMenuItem(_menus, "x-canonical-unity8-policy-kit-type", "error");

    std::shared_ptr<GMenuItem> item;

    if (index == -1)
    {
        /* Build it */
        item = shared_gobject<GMenuItem>(g_menu_item_new(error.c_str(), nullptr));
        g_menu_item_set_attribute_value(item.get(), "x-canonical-unity8-policy-kit-type",
                                        g_variant_new_string("error"));
    }
    else
    {
        /* Update it */
        item = shared_gobject<GMenuItem>(g_menu_item_new_from_model(G_MENU_MODEL(_menus.get()), index));
        g_menu_item_set_label(item.get(), error.c_str());
        g_menu_remove(_menus.get(), index);
    }

    int location = 0;
    if (g_menu_model_get_n_items(G_MENU_MODEL(_menus.get())) > 1)
    {
        location = 1;
    }

    g_menu_insert_item(_menus.get(), location, item.get());
}

void Authentication::addRequest(const std::string &request, bool password)
{
    /* If we're showing one and we get a request, uhm,
       that is weird. But let's just clear it and start
       again. */
    if (_notification)
        hideNotification();

    /* Fix menu item */
    int index = findMenuItem(_menus, "x-canonical-type", "com.canonical.snapdecision.textfield");

    std::string label;
    if (request == "password:" || request == "Password:")
    {
        label = "Password";  // TODO: Add Username
    }
    else
    {
        label = request;
    }

    if (index == -1)
    {
        /* Build it */
        auto item = shared_gobject<GMenuItem>(g_menu_item_new(label.c_str(), "pk.response"));
        g_menu_item_set_attribute_value(item.get(), "x-canonical-type",
                                        g_variant_new_string("com.canonical.snapdecision.textfield"));
        g_menu_append_item(_menus.get(), item.get());
    }
    else
    {
        /* Update it */
        auto item = shared_gobject<GMenuItem>(g_menu_item_new_from_model(G_MENU_MODEL(_menus.get()), index));
        g_menu_item_set_label(item.get(), label.c_str());
        g_menu_remove(_menus.get(), index);
        g_menu_insert_item(_menus.get(), index, item.get());
    }

    /* Build it and show it */
    _notification = buildNotification();
    showNotification();
}

void Authentication::issueCallback(Authentication::State state)
{
    /* Ensure that the callback is sent only
       once. We call this in the destructor to
       ensure that it is called at least once. */
    if (_callbackSent)
        return;

    _finishedCallback(state);
    _callbackSent = true;
}
