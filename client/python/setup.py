from setuptools import setup, find_packages

source_path = 'lib'
packages = find_packages(source_path)

setup(name='pyfpgaclient',
      version='1.0',
      packages=packages,
      package_dir={'': source_path},
     )
