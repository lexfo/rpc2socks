#!/usr/bin/env python3
#
# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import os.path
import setuptools


def read_text(*path):
    """
    Read a text file and return its content as a `str` object.

    *path* must be relative to the path of this script.
    Multiple path components may be passed.
    """
    if isinstance(path, str):
        path = (path, )

    file = os.path.join(os.path.dirname(__file__), *path)

    with open(file, mode="rt", encoding="utf-8", errors="strict") as fin:
        return fin.read()


def main():
    readme = read_text("..", "README.rst")
    metadata = {}
    exec(read_text("rpc2socks", "__meta__.py"), metadata)

    setuptools.setup(
        name=metadata["__title__"],
        version=metadata["__version__"],
        description=metadata["__description__"],
        long_description=readme,
        # long_description_content_type="text/x-rst; charset=UTF-8",
        author=metadata["__author__"],
        author_email=metadata["__author_email__"],
        url=metadata["__url__"],
        license=metadata["__license__"],

        # https://pypi.org/classifiers/
        classifiers=[
            "Development Status :: 4 - Beta",
            "License :: OSI Approved :: BSD License",
            "Operating System :: OS Independent",
            "Programming Language :: Python",
            "Programming Language :: Python :: 3"],

        python_requires=">=3.6",
        zip_safe=True,

        packages=["rpc2socks"],
        package_dir={},
        include_package_data=True,

        entry_points={
            "console_scripts": [
                "rpc2socks=rpc2socks.cmd.rpc2socks:main"]},

        # rpc2socks implements many workarounds to impacket's issues so it is
        # best to freeze impacket version in order to avoid any potential
        # trouble due to a patch changing lib's behavior
        install_requires=["impacket==0.9.21"],
        extras_require={})


if __name__ == "__main__":
    main()
