# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import subprocess
import pathlib

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = 'IPC Toolkit'
copyright = '2020, IPC Group'
author = 'IPC Group'

# -- General configuration ---------------------------------------------------

# Doxygen
pathlib.Path("../build/doxyoutput").mkdir(parents=True, exist_ok=True)
if(not subprocess.run(["doxygen", "Doxyfile"])):
    print("Doxygen failed! Exiting")
    exit(1)

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.napoleon',
    'sphinx.ext.intersphinx',
    'sphinx.ext.autosectionlabel',
    'sphinx.ext.todo',
    'sphinx.ext.mathjax',
    'sphinx.ext.ifconfig',
    'sphinx.ext.viewcode',
    'sphinx.ext.inheritance_diagram',
    'breathe',
    'm2r2',
    'nbsphinx'
]

# Setup the breathe extension
breathe_projects = {
    project: "../build/doxyoutput/xml"
}
breathe_default_project = project
breathe_default_members = ('members', 'undoc-members')

# Tell sphinx what the primary language being documented is.
primary_domain = 'cpp'

# Tell sphinx what the pygments highlight language should be.
highlight_language = 'cpp'

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
html_theme = 'alabaster'

# Theme options are theme-specific and customize the look and feel of a theme
# further.  For a list of options available for each theme, see the
# documentation.
html_theme_options = {
    "description": ("A set of reusable functions to integrate IPC into an "
                    "existing simulation."),
    "logo": "teaser@0_3.png",
    "logo_name": True,
    "github_banner": False,
    "github_button": True,
    "github_repo": "ipc-toolkit",
    "github_user": "ipc-sim",
    "github_type": "star",
    "fixed_sidebar": False,
    "page_width": "1200px",
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

# These paths are either relative to html_static_path
# or fully qualified paths (eg. https://...)
html_css_files = ['css/custom.css']
