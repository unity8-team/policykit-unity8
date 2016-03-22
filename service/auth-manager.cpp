
#include "auth-manager.h"
#include "authentication.h"

#include <libnotify/notify.h>

AuthManager::AuthManager()
{
    thread.executeOnThread([]() {
        /* Initialize Libnotify */
        auto initsuccess = notify_init("unity8-policy-kit");

        if (initsuccess == FALSE)
            throw std::runtime_error("Unable to initalize libnotify");

        /* Ensure the server has what we need */
        auto caps = notify_get_server_caps();
        bool hasDialogs = false;
        for (auto cap = caps; cap != nullptr; cap = g_list_next(cap))
        {
            auto capname = reinterpret_cast<const gchar *>(cap->data);

            if (std::string(capname) == "x-canonical-private-synchronous")  // TODO: Lookup name
                hasDialogs = true;
        }
        g_list_free_full(caps, g_free);

        if (!hasDialogs)
            throw std::runtime_error("Notification server doesn't have the capability to show dialogs!");

    });
}

AuthManager::~AuthManager()
{
    thread.executeOnThread<bool>([this]() {
        /* Cancel our authentications */
        while (!inFlight.empty())
        {
            /* Use a while because we modify the list internally */
            cancelAuthentication((*inFlight.begin()).first);
        }

        /* Uninitialize libnotify for the system */
        notify_uninit();
        return true;
    });
}

std::string AuthManager::createAuthentication(const std::string &action_id,
                                              const std::string &message,
                                              const std::string &icon_name,
                                              const std::string &cookie,
                                              const std::list<std::string> &identities,
                                              const std::function<void(Authentication::State)> &finishedCallback)
{
    return thread.executeOnThread<std::string>([this, &action_id, &message, &icon_name, &cookie, &identities,
                                                &finishedCallback]() {
        /* Build the authentication object */
        auto auth = std::make_shared<Authentication>(
            action_id, message, icon_name, cookie, identities,
            [this, cookie, finishedCallback](Authentication::State state) {
                /* Put the remove on the idle stack for the mainloop so that the
                   callback can get processed before we drop our reference */
                this->thread.timeout(std::chrono::hours{0},
                                     [this, cookie]() {
                                         auto entry = inFlight.find(cookie);

                                         if (entry == inFlight.end())
                                         {
                                             throw std::runtime_error("Handle for Authentication '" + cookie +
                                                                      "' isn't found in 'inFlight' authentication map");
                                         }

                                         inFlight.erase(entry);
                                     });

                /* Up the chain */
                finishedCallback(state);
            });

        /* Throw it in our queue */
        inFlight[cookie] = auth;

        return cookie;
    });
}

bool AuthManager::cancelAuthentication(const std::string &handle)
{
    return thread.executeOnThread<bool>([this, &handle]() {
        auto entry = inFlight.find(handle);
        if (entry == inFlight.end())
            return false;

        /* This should change the state which will cause it to be
           dropped from the inFlight map */
        (*entry).second->cancel();

        return true;
    });
}
