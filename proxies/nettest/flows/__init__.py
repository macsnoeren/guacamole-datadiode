# Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
# Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""nettest flow registry. Each flow is a callable(ctx) -> result dict living in
its own module; the web UI builds one button per entry, so adding a flow is one
module plus one line here. Shared protocol constants and helpers live in
flows.common.
"""

from .custom import run_custom

TESTS = {
    "custom": run_custom,
}
