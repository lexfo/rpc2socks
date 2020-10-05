=========
rpc2socks
=========

*rpc2socks* is a client-server solution that allows to drop and remotely run a
custom RPC + SOCKS-through-SMB server application on a *Windows* target, from a
*Unix* or *Windows* host.

The client-server pair can be used as a regular SOCKS5 tunnel (see dedicated
section below).

Client and server communicate exclusively through a dedicated named pipe over
SMB_, using a custom and extensible protocol.

The protocol, as well as the client and server sides can welcome additional
so-called *RPC commands*.

*rpc2socks-client* is a Python3 package and an interactive script that use
impacket_ to communicate with the server-side through SMB protocol (named pipe).

*rpc2socks-server* is implemented in C++ as a native, statically-linked,
*Windows* *console application* (or a *Service* if enabled at compile-time),
supporting both 32-bit and 64-bit architectures.


SOCKS tunnel
============

SOCKS-through-SMB tunnel is the main feature of *rpc2socks*.

Properties:

* SOCKS v5 only
* TCP only
* DNS resolution through SOCKS supported
* No authentication, however *user-password* auth can be easily added in
  *rpc2socks-server* if needed

It is implemented in such a way that it allows to bounce via a *Windows* host
except that the SOCKS proxy here takes its client-side packets from the named
pipe connection established between *rpc2socks-client* and *rpc2socks-server*.
With the SOCKS clients communicating directly with *rpc2socks-client*.

Thus, *rpc2socks-server* does **not** listen on a TCP socket.

::

      +---------+
      | firefox |
      +-+--+--+-+
        ^  ^  ^
        |  |  |  [SOCKS5 over TCP]
        v  v  v
  +-----+--+--+------+
  |    rpc2socks     |
  |    client CLI    |
  | (unix / windows) |
  +--------+---------+
           ^
           |  [ rpc2socks-proto  ]
           |  [through named pipe]
           |  [      (SMB)       ]
           v
     +-----+-----+
     | rpc2socks |
     |  server   |
     | (windows) |
     +--+--+--+--+
        ^  ^  ^
        |  |  |  [TCP]
        v  v  v
   +----+--+--+----+
   | The Internets |
   +---------------+


Note that the ``rpc2socks-proto`` transport depicted here consists of a single
logical link between *rpc2socks-client* and *rpc2socks-server*, regardless of
the number of established SOCKS connections and clients.

As a side note, the actual logic of the SOCKS proxy is implemented in
*rpc2socks-server*. *rpc2socks-client* only acts as a bridge between the SOCKS
client(s) and *rpc2socks-server*.


Requirements
============

Client-side requires Python 3.6+ (Unix or Windows) and use impacket_ as its sole
dependency.

Server-side native statically compiled executable requires at least Windows
Vista or Windows Server 2008.


Installation of the Client
==========================

*rpc2socks* client part is located under the ``cli/`` directory.

Although it can be run directly by using the ``bin/rpc2socks`` script (both Unix
and Windows compatible) - which may work already depending on your environment
and as long as impacket dependency is installed - it is safier to have
*rpc2socks* client installed in its own Python virtual environment.

This is because it implements many workarounds to impacket_'s issues so impacket
version is frozen in ``setup.py`` in order to avoid any trouble due to a
potential future patch changing lib's behavior.

* Fetch *rpc2socks* from git-where::

  $ git clone https://example.com/git/rpc2socks.git rpc2socks-git

* Create and activate a virtualenv::

  $ python3 -m venv rpc2socks-venv
  $ . ./rpc2socks-venv/bin/activate
  (rpc2socks-venv) $

* Install *rpc2socks*::

  (rpc2socks-venv) $ python3 -m pip install -e rpc2socks-git/cli/

* Check *rpc2socks* script is installed::

  (rpc2socks-venv) $ rpc2socks --help


Typical usage flow
==================

* Launch *rpc2socks* client CLI by giving it the required credentials to the
  remote Windows target:
::

    (rpc2socks-venv) $ rpc2socks BLOCKCHAINDOMAIN/Carlos@10.1.2.3
    Password for "Carlos":  <TYPE YOUR PASSWORD>

      This is rpc2socks prompt. "?" for help.

    rpc2socks>>>

* Use the ``inst`` command to copy the server-side executable that is embedded
  in the client, to the target through SMB, then remote-execute it using WMI,
  then connect:
::

    rpc2socks>>> inst
    10:14:56.114 checking first if remote exe is not already installed and running
    10:15:01.219 remote exe does not seem to be running
    10:15:01.220 querying host's arch
    10:15:01.234 extracting embedded 32-bit exe
    10:15:01.269 dropping 32-bit exe to \\10.1.2.3\ADMIN$\winlfo32.exe
    10:15:02.367 created remote process 3988
    10:15:02.368 remote execution successful
    10:15:04.869 trying to connect
    10:15:04.869 creating bridge
    10:15:04.872 waiting for connection to \\10.1.2.3[445]\pipe\winlfo32
    10:15:05.211 connected to \\10.1.2.3[445]\pipe\winlfo32
    10:15:05.211 bridge connected

* Note that in case WMI is not accessible, you may want to try the ``inst svc``
  command instead for an SMB-only method. Executable will be copied though SMB,
  then installed then executed as a service via the Windows Service Manager
  (still through SMB).

* At any time you can use the ``st`` command (status) to check if connection is
  still established:
::

    rpc2socks>>> st
    PROTO: connected to \\10.1.2.3[445]\pipe\winlfo32
    SOCKS: listening on 127.0.0.1:8889

While running and connected to the server-side, *rpc2socks-client* also listens
locally on a TCP port to serve as a SOCKS proxy. It listen on ``localhost:8889``
by default (see ``--socksbind`` command line option).

* Unless you need to reconnect later on (with the ``co`` command), use the
  ``uninst`` command to stop remote executable and remove it from filesystem
::

    rpc2socks>>> uninst
    10:18:10.581 sending UNSINSTALL command
    10:18:10.594 disconnected from \\10.1.2.3[445]\pipe\winlfo32
    10:18:12.584 waiting for bridge to close
    10:18:13.088 bridge closed
    10:18:15.591 deleting remote exe

* Leave the prompt using the ``q`` command (quit)
::

    rpc2socks>>> q

    (rpc2socks-venv) $


Known issues
============

High speed download
-------------------

High speed download of a **single and big data chunk** (more than a few dozens
of MiB) via the SOCKS tunnel may crash *rpc2socks-server*.

In some conditions - depending on the running environment on both ends, the
connectivity and the size of the requested data chunk - to download data through
the SOCKS tunnel of *rpc2socks* at a **high speed** rate may cause too much data
to be **buffered** by *rpc2socks* server-side, due to the client-side not being
able to follow up at that speed.

In a worst case scenario, this may lead to an unsubtle skyrocketing of the
memory usage of the server-side until shameless crash.

For now, it is strongly advised to limit the download speed of your SOCKS client
to 5MiB/s maximum - preferably even lower to stay on the safe side - in case
you require a download of more than a few dozens of MiB at once.

Side note: ideally an automatic throttling mechanism should be implemented on
the server-side. An extra development time that R&D department could not afford
at that moment :)


Double link, One client
-----------------------

More of a FYI for a significant issue that occurred during the development due
to some impacket_ flaws than an actual issue at usage-level.

Because of the reasons described below in the *impacket integration* section,
and in order to deal with *impacket*'s I/O limited capabilities,
*rpc2socks-client* **connects to two instances of the same named pipe** created
by *rpc2socks-server*. Where one instance is for read-only and the other one for
write-only operations, both in blocking mode on client side, with dedicated
Python threads and with no (reasonable) timeout so that even SMB transactions
that lead to a ``PENDING`` state can be successful.

A handshake step takes place at connection-time to "configure" each channel and
pair them under a single, well identified "logical connection". This is part of
*rpc2socks-proto* logic and makes the code more complex on both ends.


Notes to maintainers
====================

Compile *rpc2socks-server*
--------------------------

*rpc2socks-server* is built with Visual Studio 2019.

Solution file is ``svc/rpc2socks.sln``.

No other version nor compiler tested. Compiler must be C++17 aware.


Embed *server* executables
--------------------------

In case *rpc2socks-server* source code gets updated, its newly built executables
(32 and 64-bit; console and service) must be re-embedded in *rpc2socks-client*
source code using script ``tools/embedsrv.py``.

This feature is for end-user's convenience so that they do not need to build
*rpc2socks-server* themselves.

Steps:

1. Open ``svc/rpc2socks.sln`` solution file with Visual Studio 2019

2. Batch build the 4 configurations of *rpc2socks-server*; that is
   ``ConRelease`` and ``SvcRelease`` configurations for both ``x86`` and ``x64``
   architectures

3. Run ``python3 tools/embedsrv.py``. That will update
   ``cli/rpc2socks/embexe_data.py``.

4. Commit


*impacket* integration
----------------------

This section exists so to keep a feedback of the integration of *impacket* since
it has proven itself cumbersome to be integrated in an asynchronous I/O paradigm
in a reliable way and in a project bigger than a single script.

*rpc2socks* I/O have been made more complex than it should be on both client and
server sides, for the sole purpose of working around some significant issues
found in impacket_.

**I/O:** most if not all *impacket*'s I/O is blocking, which forces the
integrator to create as many threads as required connections since *impacket*
does not give access to the lower layers (the sockets) from a high level class
like ``impacket.smbconnection.SMBConnection`` for instance.

As a result in *rpc2socks-client*, two named pipe connections are created - thus
two threads dedicated to low-level I/O - and virtually paired so that they can
handle respectively read and write operations simultaneously.

**SMB:** regarding files I/O over SMB - that includes named pipes - *impacket*
does not deal correctly with SMB's ``PENDING`` response to a ``READ`` request by
not canceling the transaction then leaving the caller unnoticed about this
specific state in case of a timeout (see ``impacket.smb3.SMB3.recvSMB``).
Making any subsequent ``READ`` request to fail and to end up with the *Windows*
host forcefully terminating the connection due to protocol violation.

It is worth noting that many *impacket*'s SMB-related example scripts setup huge
timeout delays for I/Os - e.g. 100'000 seconds - as a workaround to the issue
described above, which is okay'ish for a single script run manually but hardly
practicable in bigger projects as timeouts should be handled as being part of
I/O interactions.

**Side note:** *impacket*'s internal state can be spread out commando-style in a
Maniac Mansion - i.e. bloc-level vs. instance-level vs. class-level vs.
global-level - and its way of dealing with errors may vary across multiple
implementations of a same method. See the respective methods of ``SMB`` and
``SMB3`` classes for instance.


License
=======

*rpc2socks* is released under the terms of the BSD-3-Clause license with the
following addendum::

  If we meet some day, and you think this stuff is worth it, you can buy the
  author a beer in return.

See the ``LICENSE`` file at the root directory of this project.


Contact Us
==========

We are `Lexfo <https://www.lexfo.fr/en/>`_ 👋

Twitter: https://twitter.com/LexfoSecurite

GitHub: https://github.com/lexfo


.. _SMB: https://en.wikipedia.org/wiki/Server_Message_Block
.. _impacket: https://github.com/SecureAuthCorp/impacket
