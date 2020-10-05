# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

# package info
from .__meta__ import (
    __version__, version_info,
    __title__, __description__, __license__,
    __url__, __author__, __author_email__)

# logging - initialize root and default loggers as soon as possible
from .utils import logging

# logging - aliases for high-level features
logger = logging.get_internal_logger()
set_root_log_level = logging.set_root_log_level

# expose base features from third-parties
from .utils import vendor

# expose base features
from .utils import *
from .utils import cmdkeyint
from .utils import dispatcher
from .utils import tcpserver
from .utils import threadpool

# global constants
from . import constants
from .constants import *

# protocol codec
from . import proto

# smb-related tools (impacket wrapper)
from . import smb
from .smb import *

# wmi-related tools (impacket wrapper)
from . import wmi
from .wmi import *

# tools related to windows service manager via DCERPC (impacket wrapper)
from . import svcmgr
from .svcmgr import *

# utils to extract emebexe_data
from . import embexe
from .embexe import *

# remote named pipe client
# * uses *smb* to connect
# * includes optional support of the lowest layer of *proto*, so to
#   automatically negotiate *proto*'s connection configuration step
from . import namedpipeclient
from .namedpipeclient import *

# *proto* client logic
# * it connects to the remote server-side using *namedpipeclient*
# * it acts as a relay for SOCKS-over-TCP clients
from . import bridge
from .bridge import *

# CLI to *bridge* using python's cmd.Cmd
from . import bridgecli
from .bridgecli import *
