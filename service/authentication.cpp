
#include "authentication.h"

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
{
}

Authentication::~Authentication()
{
}

void Authentication::cancel()
{
    _finishedCallback(Authentication::State::CANCELLED);
}
