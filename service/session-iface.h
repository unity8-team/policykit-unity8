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

class Session
{
public:
    Session(const std::string &identity, const std::string &cookie);
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
