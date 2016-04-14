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

#include <glib/gi18n.h>
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
Authentication::Authentication(const std::string &in_action_id,
                               const std::string &in_message,
                               const std::string &in_icon_name,
                               const std::string &in_cookie,
                               const std::list<std::string> &in_identities,
                               const std::function<void(State)> &in_finishedCallback)
    : action_id(in_action_id)
    , message(in_message)
    , icon_name(in_icon_name)
    , cookie(in_cookie)
    , identities(in_identities)
    , finishedCallback(in_finishedCallback)
{
    GError *error = nullptr;

    /* Get the Bus */
    sessionBus = shared_gobject<GDBusConnection>(g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error));
    check_error(error, "Unable to get session bus");

    /* Build a unique path */
    static unsigned int authentication_count = 0;
    auto thiscnt = ++authentication_count;
    dbusPath = "/com/canonical/unity8/policykit/authentication" + std::to_string(thiscnt);
    g_debug("DBus Path: %s", dbusPath.c_str());

    /* Setup Actions */
    actions = shared_gobject<GSimpleActionGroup>(g_simple_action_group_new());
    auto pwaction =
        shared_gobject<GSimpleAction>(g_simple_action_new_stateful("response", nullptr, g_variant_new_string("")));
    g_action_map_add_action(G_ACTION_MAP(actions.get()), G_ACTION(pwaction.get()));

    actionsExport = g_dbus_connection_export_action_group(sessionBus.get(), dbusPath.c_str(),
                                                          G_ACTION_GROUP(actions.get()), &error);
    check_error(error, "Unable to export actions");

    /* Setup Menus */
    menus = shared_gobject<GMenu>(g_menu_new());
    menusExport =
        g_dbus_connection_export_menu_model(sessionBus.get(), dbusPath.c_str(), G_MENU_MODEL(menus.get()), &error);
    check_error(error, "Unable to export menu model");
}

Authentication::~Authentication()
{
    /* This will cancel if we haven't already sent a
       complete message to the creator */
    cancel();

    if (menusExport != 0)
        g_dbus_connection_unexport_menu_model(sessionBus.get(), menusExport);
    if (actionsExport != 0)
        g_dbus_connection_unexport_action_group(sessionBus.get(), actionsExport);
}

/** Used to start the session working, split out from the constructor
    so that we can separate the two in the test suite. */
void Authentication::start(void)
{
    /** TODO: We should have an identity selector, not a requirement yet. */
    session = buildSession(*identities.begin());
}

/** Build the notification object along with all the hints that are
    required to be rather complex GVariants. */
std::shared_ptr<NotifyNotification> Authentication::buildNotification(void)
{
    /* Build our notification */
    auto notification = shared_gobject<NotifyNotification>(
        notify_notification_new(_("Elevated permissions required"), message.c_str(), icon_name.c_str()));
    if (!notification)
        throw std::runtime_error("Unable to setup notification object");

    notify_notification_set_timeout(notification.get(), NOTIFY_EXPIRES_NEVER);
    notify_notification_add_action(notification.get(), "okay", _("Login"), notification_action_response, this,
                                   nullptr); /* free func */
    notify_notification_add_action(notification.get(), "cancel", _("Cancel"), notification_action_cancel, this,
                                   nullptr); /* free func */

    g_signal_connect(notification.get(), "closed", G_CALLBACK(notification_closed), this);

    /* Set Notification hints */
    notify_notification_set_hint(notification.get(), "x-canonical-snap-decisions", g_variant_new_string("true"));

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE("{sv}"));
    g_variant_builder_add_value(&builder, g_variant_new_string("busName"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
    g_variant_builder_add_value(&builder, g_variant_new_string(g_dbus_connection_get_unique_name(sessionBus.get())));
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

/** Builds a session object from an identity and a cookie. After building
    it connects to all the signals and passes their calls to the appropriate
    function on the Authentication object.

    \param identity A PK identity string
    \param cookie An unique identifier for this authentication
*/
std::shared_ptr<Session> Authentication::buildSession(const std::string &identity)
{
    g_debug("Building a new PK session");
    auto lsession = std::make_shared<Session>(identity, cookie);

    lsession->request().connect([this](const std::string &prompt, bool password) { addRequest(prompt, password); });

    lsession->info().connect([this](const std::string &info) { setInfo(info); });

    lsession->error().connect([this](const std::string &error) { setError(error); });

    lsession->complete().connect([this](bool success) {
        hideNotification();

        if (success)
        {
            issueCallback(Authentication::State::SUCCESS);
        }
        else
        {
            /* If we're not successful we'll try again */
            session = buildSession(*identities.begin());
        }
    });

    lsession->initiate();

    return lsession;
}

/** Show a notification to the user, may include building it if it
    has been built previously. */
void Authentication::showNotification()
{
    if (!notification)
        notification = buildNotification();

    if (!notification)
        return;

    g_debug("Showing Notification");

    GError *error = nullptr;
    notify_notification_show(notification.get(), &error);
    check_error(error, "Unable to show notification");
}

/** Hide a notification. This includes closing it if open and free'ing
    the _notification variable. It also will reset the response action
    and remove all the items from the menu. */
void Authentication::hideNotification()
{
    /* Close the notification */
    if (notification)
        notify_notification_close(notification.get(), nullptr);
    notification.reset();

    /* Clear the menu */
    if (menus)
        g_menu_remove_all(G_MENU(menus.get()));

    /* Clear the response */
    if (actions)
    {
        auto action = g_action_map_lookup_action(G_ACTION_MAP(actions.get()), "response"); /* No transfer */
        if (action != nullptr && G_IS_SIMPLE_ACTION(action))
            g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_string(""));
    }
}

/** Cancel the authentication. Hide the notification if visiable and call
    the callback. */
void Authentication::cancel()
{
    g_debug("Notification Cancelled");
    hideNotification();
    issueCallback(Authentication::State::CANCELLED);
}

/** Checks the response from the user by looking at the response action and
    then passes the value to the Session object */
void Authentication::checkResponse()
{
    /* Get the password */
    auto vresponse = g_action_group_get_action_state(G_ACTION_GROUP(actions.get()), "response");
    std::string response(g_variant_get_string(vresponse, nullptr));
    g_variant_unref(vresponse);

    g_debug("Notification response: %s", response.c_str());

    hideNotification();

    session->requestResponse(response);
}

/** Find a menu item in a menu that has a specific value on an attribute */
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

/** Set the info string to show the user. If there is no info menu item
    then one is created for the information. If there is currently one it
    will be updated to be the new string */
void Authentication::setInfo(const std::string &info)
{
    int index = findMenuItem(menus, "x-canonical-unity8-policy-kit-type", "info");

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
        item = shared_gobject<GMenuItem>(g_menu_item_new_from_model(G_MENU_MODEL(menus.get()), index));
        g_menu_item_set_label(item.get(), info.c_str());
        g_menu_remove(menus.get(), index);
    }

    g_menu_prepend_item(menus.get(), item.get());
}

/** Set the error string to show the user. If there is no error menu item
    then one is created for the information. If there is currently one it
    will be updated to be the new string */
void Authentication::setError(const std::string &error)
{
    int index = findMenuItem(menus, "x-canonical-unity8-policy-kit-type", "error");

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
        item = shared_gobject<GMenuItem>(g_menu_item_new_from_model(G_MENU_MODEL(menus.get()), index));
        g_menu_item_set_label(item.get(), error.c_str());
        g_menu_remove(menus.get(), index);
    }

    int location = 0;
    if (g_menu_model_get_n_items(G_MENU_MODEL(menus.get())) > 1)
    {
        location = 1;
    }

    g_menu_insert_item(menus.get(), location, item.get());
}

/** Add a request for information from the user. This is a menu item in
    the menu model. If there isn't an item, it is created here, else it
    is updated to include this request. */
void Authentication::addRequest(const std::string &request, bool password)
{
    /* If we're showing one and we get a request, uhm,
       that is weird. But let's just clear it and start
       again. */
    if (notification)
        hideNotification();

    /* Fix menu item */
    int index = findMenuItem(menus, "x-canonical-type", "com.canonical.snapdecision.textfield");

    std::string label;
    if (request == "password:" || request == "Password:")
    {
        label = _("Password");  // TODO: Add Username (Password for Joe)
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
        g_menu_append_item(menus.get(), item.get());
    }
    else
    {
        /* Update it */
        auto item = shared_gobject<GMenuItem>(g_menu_item_new_from_model(G_MENU_MODEL(menus.get()), index));
        g_menu_item_set_label(item.get(), label.c_str());
        g_menu_remove(menus.get(), index);
        g_menu_insert_item(menus.get(), index, item.get());
    }

    /* Build it and show it */
    notification = buildNotification();
    showNotification();
}

/** Sends the callback, once and only once. It ensures that we don't
    call it multiple times and that it exits. */
void Authentication::issueCallback(Authentication::State state)
{
    /* Ensure that the callback is sent only
       once. We call this in the destructor to
       ensure that it is called at least once. */
    if (callbackSent)
        return;

    /* Check to ensure we were given a valid callback
       and then call it. */
    if (finishedCallback)
        finishedCallback(state);

    callbackSent = true;
}
