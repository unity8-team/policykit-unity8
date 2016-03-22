
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

static void agent_glib_init(AgentGlib *agent)
{
}

static void agent_glib_class_init(AgentGlibClass *klass)
{
    PolkitAgentListenerClass *listener = POLKIT_AGENT_LISTENER_CLASS(klass);

    listener->initiate_authentication = [](PolkitAgentListener *agent_listener, const gchar *action_id,
                                           const gchar *message, const gchar *icon_name, PolkitDetails *details,
                                           const gchar *cookie, GList *identities, GCancellable *cancellable,
                                           GAsyncReadyCallback callback, gpointer user_data) {
        auto agentglib = reinterpret_cast<AgentGlib *>(agent_listener);

        /* Turn identities into an STL list */
        std::list<std::string> idents;
        for (GList *identhead = identities; identhead != nullptr; identhead = g_list_next(identhead))
        {
            auto ident = reinterpret_cast<PolkitIdentity *>(identhead->data);
            auto identstr = polkit_identity_to_string(ident);
            idents.push_back(identstr);
            g_free(identstr);
        }

        /* Get a C++ shared ptr for cancellable */
        auto cancel = std::shared_ptr<GCancellable>(reinterpret_cast<GCancellable *>(g_object_ref(cancellable)),
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

        agentglib->cpp->authRequest(action_id, message, icon_name, cookie, idents, cancel, call);
    };

    listener->initiate_authentication_finish = [](PolkitAgentListener *listener, GAsyncResult *res,
                                                  GError **error) -> gboolean {
        return g_task_propagate_boolean(reinterpret_cast<GTask *>(res), error);
    };
}

AgentGlib *agent_glib_new(Agent *parent)
{
    auto ptr = reinterpret_cast<AgentGlib *>(g_object_new(agent_glib_get_type(), nullptr));
    ptr->cpp = parent;
    return ptr;
}
