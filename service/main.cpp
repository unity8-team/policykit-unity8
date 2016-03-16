
#include "agent.h"
#include "auth-manager.h"

#include <csignal>
#include <future>

std::promise<int> retval;

int main(int argc, char* argv[])
{
    auto auths = std::make_shared<AuthManager>();
    auto agent = std::make_shared<Agent>(auths);

    std::signal(SIGTERM, [](int signal) -> void { retval.set_value(0); });

    return retval.get_future().get();
}
