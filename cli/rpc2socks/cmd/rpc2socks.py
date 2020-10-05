#!/usr/bin/env python3
#
# Copyright (c) Lexfo
# SPDX-License-Identifier: BSD-3-Clause

import argparse
import getpass
import os
import os.path
import random
import re
import string
import sys
import time
import types

import rpc2socks


def generate_random_exe_name():
    haystack = string.ascii_lowercase + string.digits

    result = []
    while len(result) < 8:
        c = random.choice(haystack)

        if len(result) < 2 and c in string.digits:
            continue

        result.append(c)

    return "".join(result) + ".exe"


def validate_exe_name(exe_name):
    if not exe_name:
        return rpc2socks.DEFAULT_EXE_NAME
    elif exe_name in ("rand", "random"):
        return generate_random_exe_name()
    else:
        rem = re.fullmatch(
            r"^([A-Z0-9\-\_]{1,28})(\.EXE)?$",
            exe_name.upper(), re.A)
        if not rem:
            raise ValueError("provided exe name is invalid")

        if not rem.group(2):
            exe_name += ".exe"

        return exe_name


def string_to_tcpbindaddr(arg):
    return rpc2socks.tcpserver.string_to_addresses(
        arg, passive=True, prefer_ipv4=True,
        socktype=rpc2socks.tcpserver.SOCK_STREAM,
        sockproto=rpc2socks.tcpserver.IPPROTO_TCP)


def parse_connection_args(parser, opts):
    ctx = types.SimpleNamespace()

    # target
    try:
        ctx.domain, ctx.username, ctx.password, ctx.rhost_name = re.match(
            "(?:(?:([^/@:]*)/)?([^@:]*)(?::([^@]*))?@)?(.*)",
            opts.target).groups("")

        # in case the password contains @
        if "@" in ctx.rhost_name:
            ctx.password = (
                ctx.password + "@" +
                ctx.rhost_name.rpartition("@")[0])

            ctx.rhost_name = ctx.rhost_name.rpartition("@")[2]

        if ctx.domain is None:
            ctx.domain = ""
    except Exception:
        parser.error(f"failed to parse <target> argument: {opts.target}")

    # rhost_addr
    if opts.targetip:
        ctx.rhost_addr = opts.targetip
    else:
        ctx.rhost_addr = ctx.rhost_name

    # remote ports
    ctx.rport_smb = opts.smbport
    ctx.rport_rpc = 135  # opts.rpcport

    # kdc_host
    ctx.kdc_host = opts.dcip

    # keytab
    ctx.hashes = opts.hashes
    ctx.aes_key = opts.aeskey
    ctx.kerberos = not not ctx.aes_key
    if opts.keytab:
        kt_field, kt_value = rpc2socks.load_kerberos_keytab(
            opts.keytab, ctx.username, ctx.domain)

        setattr(ctx, kt_field, kt_value)
        ctx.kerberos = True

    # password?
    if (not ctx.password and
            ctx.username and
            not ctx.hashes and
            not opts.nopass and
            not ctx.aes_key):
        ctx.password = getpass.getpass(f"Password for \"{ctx.username}\": ")

    return ctx


def parse_args(args):
    parser = argparse.ArgumentParser(
        allow_abbrev=False, add_help=False,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "target", metavar="TARGET",
        nargs="?",  # makes this pos arg optional
        help=(
            "Target host and credentials: "
            "[[domain/]user[:pass]@]<hostname|addr>"))

    group = parser.add_argument_group("authentication")

    group.add_argument(
        "--hashes", metavar="LMHASH:NTHASH",
        help="NTLM hashes (format is LMHASH:NTHASH)")
    group.add_argument(
        "--nopass", action="store_true",
        help="Do not ask for password (useful for --kerberos)")
    group.add_argument(
        "--kerberos", "-k", action="store_true",
        help=(
            "Use Kerberos authentication. Grabs credentials from ccache file "
            "(KRB5CCNAME) based on TARGET parameters. If valid credentials "
            "cannot be found, it will use the ones specified in the command "
            "line"))
    group.add_argument(
        "--aeskey", metavar="HEXKEY",
        help="AES key to use for Kerberos Authentication (128 or 256 bits)")
    group.add_argument(
        "--keytab",
        help="Read keys for SPN from keytab file")

    group = parser.add_argument_group("connection")

    group.add_argument(
        "--dcip", metavar="DC_IPADDR",
        help=(
            "IP Address of the domain controller. If omitted it will use the "
            "domain part (FQDN) specified in the TARGET parameter"))
    group.add_argument(
        "--targetip", metavar="TARGET_IPADDR",
        help=(
            "IP Address of the target machine. If omitted it will use whatever "
            "was specified as TARGET. This is useful when TARGET is the "
            "NetBIOS name and you cannot resolve it"))
    group.add_argument(
        "--smbport", metavar="PORT",
        default=445, type=int,
        help=(
            "Destination port to connect to SMB Service "
            "(default: %(default)s)"))
    # CAUTION: changing RPC port number from default is not well supported by
    # impacket. See rpc2socks.smb.SmbConfig.spawn_dcom_connection() for more
    # info.
    #
    # group.add_argument(
    #     "--rpcport", metavar="PORT",
    #     default=135, type=int,
    #     help=(
    #         "Destination port to connect to RPC Service "
    #         "(default: %(default)s)"))

    group = parser.add_argument_group("SOCKS relay")

    group.add_argument(
        "--socksbind", metavar="ADDRPORT",
        type=string_to_tcpbindaddr,
        default="localhost:8889", action="append",
        help=(
            "Local TCP \"host:port\" pair on which the SOCKS relay must "
            "bind and listen to (default: %(default)s). "
            "IPv4, IPv6 host addresses and host name supported. "
            "Host can be \"*\" to bind on ANY local address. "
            "IPv6 addresses must be specified in square brackets "
            "(e.g. \"[::1]:8000\"). "
            "This option can be specified multiple times. "
            "CAUTION: defaults to IPv4 if host is specified by name."))

    group = parser.add_argument_group("executable payload")

    group.add_argument(
        "--exename", metavar="EXENAME",
        default=rpc2socks.DEFAULT_EXE_NAME, type=validate_exe_name,
        help=(
            f"The name of the remote executable **AND PIPE** to be used "
            f"(extension part is removed stripped for the pipe name). "
            f"\".exe\" is automatically appended if needed. "
            f"Use \"rand\" keyword to generate a random name "
            f"(e.g. \"{generate_random_exe_name()}\"). "
            f"Default: \"%(default)s\"."))

    group.add_argument(
        "--sharename", metavar="SHARE", default=rpc2socks.DEFAULT_EXE_SHARE,
        help="The name of the remote SMB share (default: %(default)s)")

    group = parser.add_argument_group("utils")

    group.add_argument(
        "--extractexe", action="store_true",
        help="Extract all embedded executables to current directory")

    group = parser.add_argument_group("miscellaneous")

    group.add_argument(
        "--help", "-h", action="help", default=argparse.SUPPRESS,
        help="Show this help message and leave.")
    group.add_argument(
        "--verbose", "-v", action="count", default=0,
        help=(
            "Be more verbose. "
            "Can be used twice to increase the level of verbosity."))
    group.add_argument(
        "--quiet", "-q", action="store_true",
        help=(
            "Print errors only. No informational nor warning messages. "
            "Overrides --verbose. "))

    opts = parser.parse_args(args)
    del args

    # verbosity
    if opts.quiet:
        rpc2socks.set_root_log_level(rpc2socks.logging.ERROR)
    elif opts.verbose >= 3:
        # unnecessary at the moment since no log level is
        # defined below DEBUG in rpc2socks
        rpc2socks.set_root_log_level(1)
    elif opts.verbose >= 2:
        rpc2socks.set_root_log_level(rpc2socks.logging.DEBUG)
    # elif opts.verbose >= 1:
    #     rpc2socks.set_root_log_level(rpc2socks.logging.INFO)
    else:
        rpc2socks.set_root_log_level(rpc2socks.logging.INFO)  # HINFO)

    # prepare running context
    context = types.SimpleNamespace()
    context.opts = opts
    context.pipe_name = os.path.basename(
        os.path.splitext(context.opts.exename)[0])

    # smb config
    smbconfig_required = not opts.extractexe  # or any((opts.install, opts.connect, opts.uninstall))
    if not smbconfig_required:
        context.smbconfig = None
    else:
        if not opts.target:
            parser.error("TARGET argument required for the selected action(s)")

        smb_opts = parse_connection_args(parser, opts)

        context.smbconfig = rpc2socks.SmbConfig(
            username=smb_opts.username, password=smb_opts.password,
            domain=smb_opts.domain, rhost_name=smb_opts.rhost_name,
            rhost_addr=smb_opts.rhost_addr, rport_smb=smb_opts.rport_smb,
            rport_rpc=smb_opts.rport_rpc, hashes=smb_opts.hashes,
            aes_key=smb_opts.aes_key, do_kerberos=smb_opts.kerberos,
            kdc_host=smb_opts.kdc_host)

        del smb_opts

    return context


def run_extractexe(context):
    EXES = (
        {
            "label": "32-bit console exe",
            "destname": "rpc2socks32con.exe",
            "extract": lambda: rpc2socks.extract_embedded_svc_exe(
                sixty_four=False, winservice=False)},
        {
            "label": "64-bit console exe",
            "destname": "rpc2socks64con.exe",
            "extract": lambda: rpc2socks.extract_embedded_svc_exe(
                sixty_four=True, winservice=False)},
        {
            "label": "32-bit service exe",
            "destname": "rpc2socks32svc.exe",
            "extract": lambda: rpc2socks.extract_embedded_svc_exe(
                sixty_four=False, winservice=True)},
        {
            "label": "64-bit service exe",
            "destname": "rpc2socks64svc.exe",
            "extract": lambda: rpc2socks.extract_embedded_svc_exe(
                sixty_four=True, winservice=True)})

    for info in EXES:
        with open(info["destname"], mode="wb") as fout:
            data = info["extract"]()
            fout.write(data)

        rpc2socks.logger.info(
            f"{info['label']} written to {info['destname']}")

    return 0


def run_cli(context):
    # send a keep-alive packet every N second or so
    proto_keep_alive = None  # 10.0

    bridgecli = rpc2socks.BridgeCli(
        smb_config=context.smbconfig,
        pipe_name=context.pipe_name,
        rshare_name=context.opts.sharename,
        rexe_name=context.opts.exename,
        proto_keep_alive=proto_keep_alive,
        socks_bind_addrs=context.opts.socksbind)

    try:
        bridgecli.cmdloop()
    except KeyboardInterrupt:
        rpc2socks.logger.warning("caught SIGINT")
        bridgecli.do_quit("")

    return 0


def main(args=None):
    rpc2socks.reconfigure_output_streams()
    args = sys.argv[1:] if args is None else args[:]
    context = parse_args(args)

    if context.opts.extractexe:
        return run_extractexe(context)
    else:
        return run_cli(context)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        rpc2socks.logger.exception("an unhandled exception occurred")
        sys.exit(1)
