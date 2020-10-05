# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

if __name__ == "__main__":
    import sys
    from .cmd import rpc2socks as rpc2socks_cmd
    sys.exit(rpc2socks_cmd.main())
