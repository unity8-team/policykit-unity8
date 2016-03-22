
#pragma once

#include <string>

#include <core/signal.h>

class Authentication
{
public:
    enum class State
    {
        INITIALIZED,
        CANCELLED,
        SUCCESS
    };

    Authentication(const std::string &action_id,
                   const std::string &message,
                   const std::string &icon_name,
                   const std::string &cookie,
                   const std::list<std::string> &identities,
                   const std::function<void(State)> &finishedCallback);
    virtual ~Authentication();

    virtual void cancel();

private:
    std::string _action_id;
    std::string _message;
    std::string _icon_name;
    std::string _cookie;
    std::list<std::string> _identities;
    std::function<void(State)> _finishedCallback;
};
