# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import binascii
import contextlib
import bz2
import hashlib
import importlib
import io
import sys

__all__ = ("stream_embedded_svc_exe", "extract_embedded_svc_exe", )


def stream_embedded_svc_exe(*, sixty_four=False, winservice=False):
    return io.BytesIO(extract_embedded_svc_exe(
        sixty_four=sixty_four, winservice=winservice))


def extract_embedded_svc_exe(*, sixty_four=False, winservice=False):
    """Load data (`bytes`) from internal module ``embexe_data``"""

    # note: importlib.resources is 3.7+ only

    package_name = globals()["__name__"].rsplit(".", maxsplit=1)[0]

    embexe_data = importlib.import_module(package_name + ".embexe_data")

    arch = "64" if sixty_four else "32"
    exetype = "SVC" if winservice else "CON"

    display_name = f"EXE{arch}{exetype}_DATA"

    data = getattr(embexe_data, f"EXE{arch}{exetype}_DATA")
    expected_size = getattr(embexe_data, f"EXE{arch}{exetype}_SIZE")
    expected_md5 = getattr(embexe_data, f"EXE{arch}{exetype}_MD5")

    data = binascii.a2b_base64(data)
    data = bz2.decompress(data)

    if len(data) != expected_size:
        raise RuntimeError(f"size of {display_name} mismatch")

    if hashlib.md5(data).hexdigest() != expected_md5:
        raise RuntimeError(f"hash of {display_name} mismatch")

    del expected_size
    del expected_md5
    del embexe_data
    del sys.modules[package_name + ".embexe_data"]

    return data
