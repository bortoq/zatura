# -*- coding: utf-8 -*-
#
# SPDX-License-Identifier: Zlib

import os.path
import glob
import time

dirname = os.path.dirname(__file__)
files = glob.glob(os.path.join(dirname, '*.rst'))

maxdate = 0
for path in files:
    s = os.stat(path)
    maxdate = max(maxdate, s.st_mtime)

# -- General configuration ------------------------------------------------

source_suffix  = '.rst'
master_doc     = 'zatura.1'
templates_path = ['_templates']
today          = time.strftime('%Y-%m-%d', time.gmtime(maxdate))

# -- Project configuration ------------------------------------------------

project   = 'zatura'
copyright = '2009-2026, pwmt.org'
version   = '0.2.7'
release   = '0.2.7'

# -- Options for manual page output ---------------------------------------

man_pages = [
    ('zatura.1', 'zatura', 'a document viewer', ['pwmt.org'], 1),
    ('zaturarc.5', 'zaturarc', 'zatura configuration file', ['pwmt.org'], 5)
]
