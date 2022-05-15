# Copyright (C) 2022 Matthew Earl
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# 
# See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


"""Attempt to parse all demos in a zip file, such as the SDA demo archive"""


import glob
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile


DZIP_EXEC = 'dzip'
DEMO_PARSE_EXEC = 'ghost/demoparse'
TMP_DZ_PATH = '/dev/shm/temp_dz'
TMP_DEM_PATH = '/dev/shm/temp_dz_unpacked/'
FAILING_DEMS_PATH = 'bad_demos'


class ParseFailed(Exception):
    pass


def dzip_extract(dz_path, destination_dir_path):
    proc = subprocess.run([DZIP_EXEC, '-x', dz_path], capture_output=True, cwd=destination_dir_path)
    if proc.returncode != 0:
        raise Exception(f'dzip failed {proc.returncode=}')


def run_demo_parse(dem_path):
    proc = subprocess.run([DEMO_PARSE_EXEC, 'info', dem_path], capture_output=True)
    if proc.returncode != 0:
        raise ParseFailed(f'parse failed {proc.returncode=}')
    return proc.stdout.decode('utf-8').strip()


def read_demos_from_zip():
    """Given a .zip file of .dz files, extract the .dem files from within"""
    os.makedirs(TMP_DEM_PATH, exist_ok=True)
    with zipfile.ZipFile(sys.argv[1]) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            dz_name = info.filename
            if not dz_name.endswith('.dz'):
                continue
            with z.open(dz_name) as in_f, open(TMP_DZ_PATH, 'wb') as out_f:
                out_f.write(in_f.read())

            with tempfile.TemporaryDirectory(prefix=TMP_DEM_PATH) as dir_path:
                dzip_extract(TMP_DZ_PATH, dir_path)
                for dem_path in glob.glob(os.path.join(dir_path, '*.dem')):
                    yield dz_name, dem_path

os.makedirs(FAILING_DEMS_PATH, exist_ok=True)
for i, (dz_name, dem_path) in enumerate(find_lots_of_demos()):
    try:
        print(repr((i, dz_name, dem_path,
                run_demo_parse(dem_path))))
    except ParseFailed as e:
        print(i, dem_path, e, 'COPIED')
        shutil.copy(dem_path, FAILING_DEMS_PATH)

