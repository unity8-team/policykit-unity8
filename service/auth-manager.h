
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
	virtual ~AuthManager();

    typedef int AuthHandle;
    virtual AuthHandle createAuthentication(std::string action_id,
                                    std::string message,
                                    std::string icon_name,
                                    std::string cookie,
                                    std::list<std::string> identities,
                                    std::function<void(Authentication *)> finishedCallback);
    virtual void cancelAuthentication(AuthHandle handle);

private:
    std::map<AuthHandle, std::shared_ptr<Authentication>> inFlight;
};
