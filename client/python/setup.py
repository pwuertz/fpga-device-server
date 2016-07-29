#-----------------------------------------------------------------------------
# Author: Peter Würtz, TU Kaiserslautern (2016)
#
# Distributed under the terms of the GNU General Public License Version 3.
# The full license is in the file COPYING.txt, distributed with this software.
#-----------------------------------------------------------------------------

from setuptools import setup, find_packages

source_path = 'lib'
packages = find_packages(source_path)

setup(name='pyfpgaclient',
      version='1.0',
      packages=packages,
      package_dir={'': source_path},
     )
