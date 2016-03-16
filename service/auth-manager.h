
#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>

class Authentication;

class AuthManager
{
public:
    AuthManager();

    typedef int AuthHandle;
    AuthHandle createAuthentication(std::string action_id,
                                    std::string message,
                                    std::string icon_name,
                                    std::string cookie,
                                    std::list<std::string> identities,
                                    std::function<void(Authentication *)> finishedCallback);
    void cancelAuthentication(AuthHandle handle);

private:
    std::map<AuthHandle, std::shared_ptr<Authentication>> inFlight;
};
