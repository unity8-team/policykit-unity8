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

#include <core/signal.h>
#include <memory>
#include <string>

#pragma once

/** \brief An interface for the session functionality of libpolicykitagent
        so that we can mock it out.

    It is basically impossible to test against PAM without going
    crazy, so we created an interface to mock it out. This class
    makes the session interface into nice C++ objects but also
    virtualizes it so that it can be mocked.
*/
class Session
{
public:
    Session(const std::string& identity, const std::string& cookie);
    virtual ~Session();

    virtual void initiate();
    virtual void resetSession();

    virtual core::Signal<const std::string&, bool>& request();
    virtual void requestResponse(const std::string& response);

    virtual core::Signal<const std::string&>& info();
    virtual core::Signal<const std::string&>& error();
    virtual core::Signal<bool>& complete();

protected:
    Session()
    {
    }

private:
    class Impl;
    /** Someone should implement this */
    std::shared_ptr<Impl> impl;
};
