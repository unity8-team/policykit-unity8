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

#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>

#include "authentication.h"
#include "glib-thread.h"

class AuthManager
{
public:
    AuthManager();
    virtual ~AuthManager();

    virtual std::string createAuthentication(const std::string& action_id,
                                             const std::string& message,
                                             const std::string& icon_name,
                                             const std::string& cookie,
                                             const std::list<std::string>& identities,
                                             const std::function<void(Authentication::State)>& finishedCallback);
    virtual bool cancelAuthentication(const std::string& handle);

protected:
    virtual std::shared_ptr<Authentication> buildAuthentication(
        const std::string& action_id,
        const std::string& message,
        const std::string& icon_name,
        const std::string& cookie,
        const std::list<std::string>& identities,
        const std::function<void(Authentication::State)>& finishedCallback);

private:
    std::map<std::string, std::shared_ptr<Authentication>> inFlight;
    GLib::ContextThread thread;
};
