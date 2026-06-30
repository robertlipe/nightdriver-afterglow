#!/usr/bin/env python

#--------------------------------------------------------------------------
#
# File:        show_envs.py
#
# NightDriverStrip - (c) 2023 Plummer's Software LLC.  All Rights Reserved.
#
# This file is part of the NightDriver software project.
#
#    NightDriver is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    NightDriver is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Nightdriver.  It is normally found in copying.txt
#    If not, see <https://www.gnu.org/licenses/>.
#
# Description:
#
#    This script outputs a JSON array with all the environment names defined in
#    platformio.ini.
#    It is used by this project's GitHub CI workflow.
#
#    Note that it expects to be executed from the project root directory. That is,
#    it needs to be run like this:
#
#    $ tools/show_envs.py
#
#    Instead of:
#
#    $ cd tools
#    $ ./show_envs.py
#
# History:     Aug-08-2023         Rbergen      Added header
#              Aug-27-2023         Rbergen      Make importable
#
#---------------------------------------------------------------------------

import configparser
import json
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def discover_config_files():
    config_files = [PROJECT_ROOT / 'platformio.ini']

    parser = configparser.ConfigParser()
    parser.read(PROJECT_ROOT / 'platformio.ini')

    extra_configs = parser.get('platformio', 'extra_configs', fallback='')
    for line in extra_configs.splitlines():
        pattern = line.split(';', 1)[0].split('#', 1)[0].strip()
        if not pattern or pattern.startswith(';') or pattern.startswith('#'):
            continue
        config_files.extend(sorted(PROJECT_ROOT.glob(pattern)))

    unique_files = []
    seen = set()
    for path in config_files:
        resolved = path.resolve()
        if not resolved.is_file() or resolved in seen:
            continue
        seen.add(resolved)
        unique_files.append(resolved)

    return unique_files

def getenvs():
    envs = []
    for config_file in discover_config_files():
        config = configparser.ConfigParser()
        config.read(config_file)

        for section in config.sections():
            if section.startswith('env:') and len(section) > 4:
                env = section[4::]
                if env not in envs:
                    envs.append(env)

    return envs

if __name__ == '__main__':
    print(json.dumps(getenvs()))
