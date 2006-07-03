from distutils.core import setup, Extension
import sys
from glob import glob

sources = glob("*.c")
undefine = []
define = []

try:
    sys.argv.remove("--debug")
    undefine.append("NDEBUG")
    define.append(("DEBUG", 1))
except ValueError:
    pass

# This assumes that libmemcache is installed with base /usr/local
cmemcache = Extension(
    "_cmemcache",
    sources,
    include_dirs = ['/usr/local/include'],
    extra_compile_args = ['-Wall'],
    libraries=['memcache'],
    library_dirs=['/usr/local/lib'],
    extra_link_args=['--no-undefined', '-Wl,-rpath=/usr/local/lib'],
    define_macros=define,
    undef_macros=undefine)

setup(name="cmemcache",
      version="1.0",
      description="cmemcache -- memcached extension",
      long_description="cmemcache -- memcached extension built on libmemcache",
      author="Gijsbert de Haan",
      author_email="gijsbert_de_haan@hotmail.com",
      maintainer="Gijsbert de Haan",
      maintainer_email="gijsbert_de_haan@hotmail.com",
      url="http://gijsbert.org/",
      license="GNU General Public License (GPL)",
      py_modules = ['cmemcache'],
      ext_modules=[cmemcache])
