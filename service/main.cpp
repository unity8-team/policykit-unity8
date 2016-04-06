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

#include "agent.h"
#include "auth-manager.h"

#include <csignal>
#include <future>

#include <glib.h>

std::promise<int> retval;

int main(int argc, char* argv[])
{
    auto auths = std::make_shared<AuthManager>();
    auto agent = std::make_shared<Agent>(auths);

    std::signal(SIGTERM, [](int signal) -> void { retval.set_value(0); });

    g_debug("PolicyKit Agent Started");
    return retval.get_future().get();
}
