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

#include "agent-glib.h"
#include "authentication.h"

#include <list>

struct _AgentGlib
{
    PolkitAgentListener parent;
    Agent *cpp;
};

struct _AgentGlibClass
{
    PolkitAgentListenerClass parent;
};

G_DEFINE_TYPE(AgentGlib, agent_glib, POLKIT_AGENT_TYPE_LISTENER);
G_DEFINE_QUARK(AgentGlibError, agent_glib_error);

static void initiate_authentication(PolkitAgentListener *agent_listener,
                                    const gchar *action_id,
                                    const gchar *message,
                                    const gchar *icon_name,
                                    PolkitDetails *details,
                                    const gchar *cookie,
                                    GList *identities,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);

static void agent_glib_init(AgentGlib *agent)
{
}

static void agent_glib_class_init(AgentGlibClass *klass)
{
    PolkitAgentListenerClass *listener = POLKIT_AGENT_LISTENER_CLASS(klass);

    listener->initiate_authentication = initiate_authentication;
    listener->initiate_authentication_finish = [](PolkitAgentListener *listener, GAsyncResult *res,
                                                  GError **error) -> gboolean {
        return g_task_propagate_boolean(G_TASK(res), error);
    };
}

AgentGlib *agent_glib_new(Agent *parent)
{
    auto ptr = static_cast<AgentGlib *>(g_object_new(agent_glib_get_type(), nullptr));
    ptr->cpp = parent;
    return ptr;
}

static inline std::string protect_string(const gchar *instr)
{
    if (instr == nullptr)
        return {};
    else
        return std::string(instr);
}

static void initiate_authentication(PolkitAgentListener *agent_listener,
                                    const gchar *action_id,
                                    const gchar *message,
                                    const gchar *icon_name,
                                    PolkitDetails *details,
                                    const gchar *cookie,
                                    GList *identities,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{

    /* Turn identities into an STL list */
    std::list<std::string> idents;
    for (GList *identhead = identities; identhead != nullptr; identhead = g_list_next(identhead))
    {
        auto ident = static_cast<PolkitIdentity *>(identhead->data);
        auto identstr = polkit_identity_to_string(ident);
        idents.push_back(protect_string(identstr));
        g_free(identstr);
    }

    /* Get a C++ shared ptr for cancellable */
    auto cancel = std::shared_ptr<GCancellable>(static_cast<GCancellable *>(g_object_ref(cancellable)),
                                                [](GCancellable *cancel) { g_clear_object(&cancel); });
    auto task = std::shared_ptr<GTask>(g_task_new(agent_listener, nullptr, callback, user_data),
                                       [](GTask *task) { g_clear_object(&task); });

    /* Make a function object for the callback */
    auto call = [task](Authentication::State state) -> void {
        if (state == Authentication::State::CANCELLED)
        {
            g_task_return_new_error(task.get(), agent_glib_error_quark(), 0, "Authentication Error: Cancelled");
        }
        else
        {
            g_task_return_boolean(task.get(), TRUE);
        }
    };

    auto agentglib = reinterpret_cast<AgentGlib *>(agent_listener);
    agentglib->cpp->authRequest(protect_string(action_id), protect_string(message), protect_string(icon_name),
                                protect_string(cookie), idents, cancel, call);
}
