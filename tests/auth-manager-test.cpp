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

/* Test Libraries */
#include <gmock/gmock.h>
#include <gtest/gtest.h>

/* Local Headers */
#include "agent.h"
#include "auth-manager.h"
#include "authentication.h"

class AuthManagerTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
    }
};

class AuthenticationMock : public Authentication
{
public:
    AuthenticationMock(const std::string& action_id,
                       const std::string& message,
                       const std::string& icon_name,
                       const std::string& cookie,
                       const std::list<std::string>& identities,
                       const std::function<void(State)>& finishedCallback)
        : Authentication()
        , _action_id(action_id)
        , _message(message)
        , _icon_name(icon_name)
        , _cookie(cookie)
        , _identities(identities)
        , _finishedCallback(finishedCallback)
    {
    }

    virtual void cancel() override
    {
        g_debug("Mock cancelled");
        _finishedCallback(Authentication::State::CANCELLED);
    }

    std::string _action_id;
    std::string _message;
    std::string _icon_name;
    std::string _cookie;
    std::list<std::string> _identities;
    std::function<void(State)> _finishedCallback;
};

class AuthManagerAuthMock : public AuthManager
{
public:
    AuthManagerAuthMock()
        : AuthManager()
    {
    }

    virtual ~AuthManagerAuthMock()
    {
    }

    std::weak_ptr<AuthenticationMock> lastMock;

    std::shared_ptr<Authentication> buildAuthentication(
        const std::string& action_id,
        const std::string& message,
        const std::string& icon_name,
        const std::string& cookie,
        const std::list<std::string>& identities,
        const std::function<void(Authentication::State)>& finishedCallback) override
    {
        g_debug("Building Mock Authentication");
        auto auth =
            std::make_shared<AuthenticationMock>(action_id, message, icon_name, cookie, identities, finishedCallback);
        lastMock = auth;
        return auth;
    }
};

TEST_F(AuthManagerTest, Init)
{
    AuthManagerAuthMock authman;
}

TEST_F(AuthManagerTest, createAuth)
{
    AuthManagerAuthMock authman;

    authman.createAuthentication("action-id", "message", "icon-name", "everyone-loves-cookies", {"unix-name:me"},
                                 [](Authentication::State state) {});
}
