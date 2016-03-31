
#include <core/signal.h>
#include <memory>
#include <string>

class Session
{
public:
    Session(const std::string &cookie);
    virtual ~Session();

    virtual void initiate();

    virtual core::Signal<const std::string &, bool> &request();
    virtual void requestResponse(const std::string &response);

    virtual core::Signal<const std::string &> &info();
    virtual core::Signal<const std::string &> &error();
    virtual core::Signal<bool> &complete();

private:
    class Impl;
    std::shared_ptr<Impl> impl;
};
