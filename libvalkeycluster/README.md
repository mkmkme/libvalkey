# Libvalkeycluster

Libvalkeycluster is a C client library for cluster deployments of the
[Valkey](https://valkey.io/) database.

Libvalkeycluster is using [Libvalkey](https://github.com/valkey-io/libvalkey) for the
connections to each Valkey node.

## Build instructions

Prerequisites:

* A C compiler (GCC or Clang)
* CMake and GNU Make (but see [Alternative build using Makefile
  directly](#alternative-build-using-makefile-directly) below for how to build
  without CMake)
* [libvalkey](https://github.com/valkey-io/libvalkey).
* [libevent](https://libevent.org/) (`libevent-dev` in Debian); can be avoided
  if building without tests (DISABLE_TESTS=ON)
* OpenSSL (`libssl-dev` in Debian) if building with TLS support

libvalkeycluster will be built as a shared library `libvalkeycluster.so` and
it depends on the libvalkey shared library `libvalkey.so`.

When SSL/TLS support is enabled an extra library `libvalkeycluster_ssl.so`
is built, which depends on the libvalkey SSL support library `libvalkey_ssl.a`.

A user project that needs SSL/TLS support should link to both `libvalkeycluster.so`
and `libvalkeycluster_ssl.so` to enable the SSL/TLS configuration API.

```sh
$ mkdir build; cd build
$ cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_SSL=ON ..
$ make
```

### Build options

The following CMake options are available:

* `ENABLE_SSL`
  * `OFF` (default)
  * `ON` Enable SSL/TLS support and build its tests.
* `DISABLE_TESTS`
  * `OFF` (default)
  * `ON` Disable compilation of tests.
* `ENABLE_IPV6_TESTS`
  * `OFF` (default)
  * `ON` Enable IPv6 tests. Requires that IPv6 is
    [setup](https://docs.docker.com/config/daemon/ipv6/) in Docker.
* `ENABLE_COVERAGE`
  * `OFF` (default)
  * `ON` Compile using build flags that enables the GNU coverage tool `gcov`
    to provide test coverage information. This CMake option also enables a new
    build target `coverage` to generate a test coverage report using
    [gcovr](https://gcovr.com/en/stable/index.html).
* `USE_SANITIZER`
   Compile using a specific sanitizer that detect issues. The value of this
   option specifies which sanitizer to activate, but it depends on support in the
   [compiler](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html#index-fsanitize_003daddress).
   Common option values are: `address`, `thread`, `undefined`, `leak`

Options needs to be set with the `-D` flag when generating makefiles, e.g.

`cmake -DENABLE_SSL=ON -DUSE_SANITIZER=address ..`

### Build details

The build uses CMake's [find_package](https://cmake.org/cmake/help/latest/command/find_package.html#search-procedure)
to search for a `libvalkey` installation. CMake will search for a `libvalkey`
installation in the default paths, searching for a file called `valkey-config.cmake`.
The default search path can be altered via `CMAKE_PREFIX_PATH` or
as described in the CMake docs; a specific path can be set using a flag like:
`-Dvalkey_DIR:PATH=${MY_DIR}/valkey/share/valkey`

See `examples/using_cmake_separate/build.sh` or
`examples/using_cmake_externalproject/build.sh` for alternative CMake builds.

### Extend the list of supported commands

The list of commands and the position of the first key in the command line is
defined in `cmddef.h` which is included in this repo. It has been generated
using the JSON files describing the syntax of each command in the Valkey
repository, which makes sure libvalkeycluster supports all commands in Valkey, at
least in terms of cluster routing. To add support for custom commands defined in
Valkey modules, you can regenerate `cmddef.h` using the script `gencommands.py`.
Use the JSON files from Valkey and any additional files on the same format as
arguments to the script. For details, see the comments inside `gencommands.py`.

### Alternative build using Makefile directly

When a simpler build setup is preferred a provided Makefile can be used directly
when building.

The only option that exists in the Makefile is to enable SSL/TLS support via `USE_SSL=1`

By default the libvalkey library (and headers) installed on the system is used,
but alternative installations can be used by defining the compiler flags
`CFLAGS` and `LDFLAGS`.

See [`examples/using_make/build.sh`](examples/using_make/build.sh) for an
example build using an alternative libvalkey installation.

Build failures like
`valkeycluster_ssl.h:33:10: fatal error: valkey/valkey_ssl.h: No such file or directory`
indicates that libvalkey is not installed on the system, or that a given `CFLAGS` is wrong.
Use the previous mentioned build example as reference.

### Running the tests

Prerequisites:

* Perl with [JSON module](https://metacpan.org/pod/JSON).
  Can be installed using `sudo cpan JSON`.
* [Docker](https://docs.docker.com/engine/install/)

Some tests needs a Valkey cluster and that can be setup by the make targets
`start`/`stop`. The clusters will be setup using Docker and it may take a while
for them to be ready and accepting requests. Run `make start` to start the
clusters and then wait a few seconds before running `make test`.
To stop the running cluster containers run `make stop`.

```sh
$ make start
$ make test
$ make stop
```

If you want to set up the Valkey clusters manually they should run on localhost
using following access ports:

| Cluster type                                        | Access port |
| ----------------------------------                  | -------:    |
| IPv4                                                | 7000        |
| IPv4, authentication needed, password: `secretword` | 7100        |
| IPv6                                                | 7200        |
| IPv4, using TLS/SSL                                 | 7300        |

## Quick usage

## Cluster synchronous API

### Connecting

The function `valkeyClusterContextInit` is used to create a `valkeyClusterContext`.
The context is where the state for connections is kept.

The function `valkeyClusterSetOptionAddNodes` is used to add one or many Valkey Cluster addresses.

The functions `valkeyClusterSetOptionUsername` and
`valkeyClusterSetOptionPassword` are used to configure authentication, causing
the AUTH command to be sent on every new connection to Valkey.

For more options, see the file [`valkeycluster.h`](valkeycluster.h).

The function `valkeyClusterConnect2` is used to connect to the Valkey Cluster.

The `valkeyClusterContext` struct has an integer `err` field that is non-zero when the connection is
in an error state. The field `errstr` will contain a string with a description of the error.
After trying to connect to Valkey using `valkeyClusterContext` you should check the `err` field to see
if establishing the connection was successful:
```c
valkeyClusterContext *cc = valkeyClusterContextInit();
valkeyClusterSetOptionAddNodes(cc, "127.0.0.1:6379,127.0.0.1:6380");
valkeyClusterConnect2(cc);
if (cc != NULL && cc->err) {
    printf("Error: %s\n", cc->errstr);
    // handle error
}
```

#### Events per cluster context

There is a hook to get notified when certain events occur.

```c
int valkeyClusterSetEventCallback(valkeyClusterContext *cc,
                                 void(fn)(const valkeyClusterContext *cc, int event,
                                          void *privdata),
                                 void *privdata);
```

The callback is called with `event` set to one of the following values:

* `VALKEYCLUSTER_EVENT_SLOTMAP_UPDATED` when the slot mapping has been updated;
* `VALKEYCLUSTER_EVENT_READY` when the slot mapping has been fetched for the first
  time and the client is ready to accept commands, useful when initiating the
  client with `valkeyClusterAsyncConnect2()` where a client is not immediately
  ready after a successful call;
* `VALKEYCLUSTER_EVENT_FREE_CONTEXT` when the cluster context is being freed, so
  that the user can free the event privdata.

#### Events per connection

There is a hook to get notified about connect and reconnect attempts.
This is useful for applying socket options or access endpoint information for a connection to a particular node.
The callback is registered using the following function:

```c
int valkeyClusterSetConnectCallback(valkeyClusterContext *cc,
                                   void(fn)(const valkeyContext *c, int status));
```

The callback is called just after connect, before TLS handshake and Valkey authentication.

On successful connection, `status` is set to `VALKEY_OK` and the valkeyContext
(defined in valkey.h) can be used, for example, to see which IP and port it's
connected to or to set socket options directly on the file descriptor which can
be accessed as `c->fd`.

On failed connection attempt, this callback is called with `status` set to
`VALKEY_ERR`. The `err` field in the `valkeyContext` can be used to find out
the cause of the error.

### Sending commands

The function `valkeyClusterCommand` takes a format similar to printf.
In the simplest form it is used like:
```c
reply = valkeyClusterCommand(clustercontext, "SET foo bar");
```

The specifier `%s` interpolates a string in the command, and uses `strlen` to
determine the length of the string:
```c
reply = valkeyClusterCommand(clustercontext, "SET foo %s", value);
```
Internally, libvalkeycluster splits the command in different arguments and will
convert it to the protocol used to communicate with Valkey.
One or more spaces separates arguments, so you can use the specifiers
anywhere in an argument:
```c
reply = valkeyClusterCommand(clustercontext, "SET key:%s %s", myid, value);
```

Commands will be sent to the cluster node that the client perceives handling the given key.
If the cluster topology has changed the Valkey node might respond with a redirection error
which the client will handle, update its slotmap and resend the command to correct node.
The reply will in this case arrive from the correct node.

If a node is unreachable, for example if the command times out or if the connect
times out, it can indicated that there has been a failover and the node is no
longer part of the cluster. In this case, `valkeyClusterCommand` returns NULL and
sets `err` and `errstr` on the cluster context, but additionally, libvalkey
cluster schedules a slotmap update to be performed when the next command is
sent. That means that if you try the same command again, there is a good chance
the command will be sent to another node and the command may succeed.

### Sending commands to a specific node

When there is a need to send commands to a specific node, the following low-level API can be used.

```c
reply = valkeyClusterCommandToNode(clustercontext, node, "DBSIZE");
```

This function handles printf like arguments similar to `valkeyClusterCommand()`, but will
only attempt to send the command to the given node and will not perform redirects or retries.

If the command times out or the connection to the node fails, a slotmap update
is scheduled to be performed when the next command is sent.
`valkeyClusterCommandToNode` also performs a slotmap update if it has previously
been scheduled.

### Teardown

To disconnect and free the context the following function can be used:
```c
void valkeyClusterFree(valkeyClusterContext *cc);
```
This function closes the sockets and deallocates the context.

### Cluster pipelining

The function `valkeyClusterGetReply` is exported as part of the Libvalkey API and can be used
when a reply is expected on the socket. To pipeline commands, the only things that needs
to be done is filling up the output buffer. For this cause, the following commands can be used that
are identical to the `valkeyClusterCommand` family, apart from not returning a reply:
```c
int valkeyClusterAppendCommand(valkeyClusterContext *cc, const char *format, ...);
int valkeyClusterAppendCommandArgv(valkeyClusterContext *cc, int argc, const char **argv);

/* Send a command to a specific cluster node */
int valkeyClusterAppendCommandToNode(valkeyClusterContext *cc, valkeyClusterNode *node,
                                    const char *format, ...);
```
After calling either function one or more times, `valkeyClusterGetReply` can be used to receive the
subsequent replies. The return value for this function is either `VALKEY_OK` or `VALKEY_ERR`, where
the latter means an error occurred while reading a reply. Just as with the other commands,
the `err` field in the context can be used to find out what the cause of this error is.
```c
void valkeyClusterReset(valkeyClusterContext *cc);
```
Warning: You must call `valkeyClusterReset` function after one pipelining anyway.

Warning: Calling `valkeyClusterReset` without pipelining first will reset all Valkey connections.

The following examples shows a simple cluster pipeline:
```c
valkeyReply *reply;
valkeyClusterAppendCommand(clusterContext,"SET foo bar");
valkeyClusterAppendCommand(clusterContext,"GET foo");
valkeyClusterGetReply(clusterContext,&reply); // reply for SET
freeReplyObject(reply);
valkeyClusterGetReply(clusterContext,&reply); // reply for GET
freeReplyObject(reply);
valkeyClusterReset(clusterContext);
```

## Cluster asynchronous API

Libvalkeycluster comes with an asynchronous cluster API that works with many event systems.
Currently there are adapters that enables support for `libevent`, `libev`, `libuv`, `glib`
and `ae`. For usage examples, see the test programs with the different
event libraries `tests/ct_async_{libev,libuv,glib}.c`.

The libvalkey library has adapters for additional event systems that easily can be adapted
for libvalkeycluster as well.

### Connecting

There are two alternative ways to initiate a cluster client which also determines
how the client behaves during the initial connect.

The first alternative is to use the function `valkeyClusterAsyncConnect`, which initially
connects to the cluster in a blocking fashion and waits for the slotmap before returning.
Any command sent by the user thereafter will create a new non-blocking connection,
unless a non-blocking connection already exists to the destination.
The function returns a pointer to a newly created `valkeyClusterAsyncContext` struct and
its `err` field should be checked to make sure the initial slotmap update was successful.

```c
// Insufficient error handling for brevity.
valkeyClusterAsyncContext *acc = valkeyClusterAsyncConnect("127.0.0.1:6379", VALKEYCLUSTER_FLAG_NULL);
if (acc->err) {
    printf("error: %s\n", acc->errstr);
    exit(1);
}

// Attach an event engine. In this example we use libevent.
struct event_base *base = event_base_new();
valkeyClusterLibeventAttach(acc, base);
```

The second alternative is to use `valkeyClusterAsyncContextInit` and `valkeyClusterAsyncConnect2`
which avoids the initial blocking connect. This connection alternative requires an attached
event engine when `valkeyClusterAsyncConnect2` is called, but the connect and the initial
slotmap update is done in a non-blocking fashion.

This means that commands sent directly after `valkeyClusterAsyncConnect2` may fail
because the initial slotmap has not yet been retrieved and the client doesn't know which
cluster node to send the command to. You may use the [eventCallback](#events-per-cluster-context)
to be notified when the slotmap is updated and the client is ready to accept commands.
An crude example of using the eventCallback can be found in [this testcase](tests/ct_async.c).

```c
// Insufficient error handling for brevity.
valkeyClusterAsyncContext *acc = valkeyClusterAsyncContextInit();

// Add a cluster node address for the initial connect.
valkeyClusterSetOptionAddNodes(acc->cc, "127.0.0.1:6379");

// Attach an event engine. In this example we use libevent.
struct event_base *base = event_base_new();
valkeyClusterLibeventAttach(acc, base);

if (valkeyClusterAsyncConnect2(acc) != VALKEY_OK) {
    printf("error: %s\n", acc->errstr);
    exit(1);
}
```

#### Events per cluster context

Use [`valkeyClusterSetEventCallback`](#events-per-cluster-context) with `acc->cc`
as the context to get notified when certain events occur.

#### Events per connection

Because the connections that will be created are non-blocking,
the kernel is not able to instantly return if the specified
host and port is able to accept a connection.
Instead, use a connect callback to be notified when a connection
is established or failed.
Similarily, a disconnect callback can be used to be notified about
a disconnected connection (either because of an error or per user request).
The callbacks are installed using the following functions:

```c
int valkeyClusterAsyncSetConnectCallback(valkeyClusterAsyncContext *acc,
                                        valkeyConnectCallback *fn);
int valkeyClusterAsyncSetDisonnectCallback(valkeyClusterAsyncContext *acc,
                                          valkeyConnectCallback *fn);
```

The callback functions should have the following prototype,
aliased to `valkeyConnectCallback`:

```c
void(const valkeyAsyncContext *ac, int status);
```

Alternatively, if `valkey` >= v1.1.0 is used, you set a connect callback
that will be passed a non-const `valkeyAsyncContext*` on invocation (e.g.
to be able to set a push callback on it).

```c
int valkeyClusterAsyncSetConnectCallbackNC(valkeyClusterAsyncContext *acc,
                                          valkeyConnectCallbackNC *fn);
```

The callback function should have the following prototype,
aliased to `valkeyConnectCallbackNC`:
```c
void(valkeyAsyncContext *ac, int status);
```

On a connection attempt, the `status` argument is set to `VALKEY_OK`
when the connection was successful.
The file description of the connection socket can be retrieved
from a valkeyAsyncContext as `ac->c->fd`.
On a disconnect, the `status` argument is set to `VALKEY_OK`
when disconnection was initiated by the user,
or `VALKEY_ERR` when the disconnection was caused by an error.
When it is `VALKEY_ERR`, the `err` field in the context can be accessed
to find out the cause of the error.

You don't need to reconnect in the disconnect callback.
Libvalkeycluster will reconnect by itself when the next command for this Valkey node is handled.

Setting the connect and disconnect callbacks can only be done once per context.
For subsequent calls it will return `VALKEY_ERR`.

### Sending commands and their callbacks

In an asynchronous cluster context, commands are automatically pipelined due to the nature of an event loop.
Therefore, unlike the synchronous API, there is only a single way to send commands.
Because commands are sent to Valkey Cluster asynchronously, issuing a command requires a callback function
that is called when the reply is received. Reply callbacks should have the following prototype:
```c
void(valkeyClusterAsyncContext *acc, void *reply, void *privdata);
```
The `privdata` argument can be used to carry arbitrary data to the callback from the point where
the command is initially queued for execution.

The most commonly used functions to issue commands in an asynchronous context are:
```c
int valkeyClusterAsyncCommand(valkeyClusterAsyncContext *acc,
                             valkeyClusterCallbackFn *fn,
                             void *privdata, const char *format, ...);
int valkeyClusterAsyncCommandArgv(valkeyClusterAsyncContext *acc,
                                 valkeyClusterCallbackFn *fn, void *privdata,
                                 int argc, const char **argv,
                                 const size_t *argvlen);
int valkeyClusterAsyncFormattedCommand(valkeyClusterAsyncContext *acc,
                                      valkeyClusterCallbackFn *fn,
                                      void *privdata, char *cmd, int len);
```
These functions works like their blocking counterparts. The return value is `VALKEY_OK` when the command
was successfully added to the output buffer and `VALKEY_ERR` otherwise. When the connection is being
disconnected per user-request, no new commands may be added to the output buffer and `VALKEY_ERR` is
returned.

If the reply for a command with a `NULL` callback is read, it is immediately freed. When the callback
for a command is non-`NULL`, the memory is freed immediately following the callback: the reply is only
valid for the duration of the callback.

All pending callbacks are called with a `NULL` reply when the context encountered an error.

### Sending commands to a specific node

When there is a need to send commands to a specific node, the following low-level API can be used.

```c
int valkeyClusterAsyncCommandToNode(valkeyClusterAsyncContext *acc,
                                   valkeyClusterNode *node,
                                   valkeyClusterCallbackFn *fn, void *privdata,
                                   const char *format, ...);
int valkeyClusterAsyncCommandArgvToNode(valkeyClusterAsyncContext *acc,
                                       valkeyClusterNode *node,
                                       valkeyClusterCallbackFn *fn,
                                       void *privdata, int argc,
                                       const char **argv,
                                       const size_t *argvlen);
int valkeyClusterAsyncFormattedCommandToNode(valkeyClusterAsyncContext *acc,
                                            valkeyClusterNode *node,
                                            valkeyClusterCallbackFn *fn,
                                            void *privdata, char *cmd, int len);
```

These functions will only attempt to send the command to a specific node and will not perform redirects or retries,
but communication errors will trigger a slotmap update just like the commonly used API.

### Disconnecting

Asynchronous cluster connections can be terminated using:
```c
void valkeyClusterAsyncDisconnect(valkeyClusterAsyncContext *acc);
```
When this function is called, connections are **not** immediately terminated. Instead, new
commands are no longer accepted and connections are only terminated when all pending commands
have been written to a socket, their respective replies have been read and their respective
callbacks have been executed. After this, the disconnection callback is executed with the
`VALKEY_OK` status and the context object is freed.

### Using event library *X*

There are a few hooks that need to be set on the cluster context object after it is created.
See the `adapters/` directory for bindings to *libevent* and a range of other event libraries.

## Other details

### Cluster node iterator

A `valkeyClusterNodeIterator` can be used to iterate on all known master nodes in a cluster context.
First it needs to be initiated using `valkeyClusterInitNodeIterator()` and then you can repeatedly
call `valkeyClusterNodeNext()` to get the next node from the iterator.

```c
void valkeyClusterInitNodeIterator(valkeyClusterNodeIterator *iter,
                                  valkeyClusterContext *cc);
valkeyClusterNode *valkeyClusterNodeNext(valkeyClusterNodeIterator *iter);
```

The iterator will handle changes due to slotmap updates by restarting the iteration, but on the new
set of master nodes. There is no bookkeeping for already iterated nodes when a restart is triggered,
which means that a node can be iterated over more than once depending on when the slotmap update happened
and the change of cluster nodes.

Note that when `valkeyClusterCommandToNode` is called, a slotmap update can
happen if it has been scheduled by the previous command, for example if the
previous call to `valkeyClusterCommandToNode` timed out or the node wasn't
reachable.

To detect when the slotmap has been updated, you can check if the iterator's
slotmap version (`iter.route_version`) is equal to the current cluster context's
slotmap version (`cc->route_version`). If it isn't, it means that the slotmap
has been updated and the iterator will restart itself at the next call to
`valkeyClusterNodeNext`.

Another way to detect that the slotmap has been updated is to [register an event
callback](#events-per-cluster-context) and look for the event
`VALKEYCLUSTER_EVENT_SLOTMAP_UPDATED`.

### Random number generator

This library uses [random()](https://linux.die.net/man/3/random) while selecting
a node used for requesting the cluster topology (slotmap). A user should seed
the random number generator using [srandom()](https://linux.die.net/man/3/srandom)
to get less predictability in the node selection.

### Allocator injection

Libvalkeycluster uses libvalkey's allocation structure with configurable allocation and deallocation functions. By default they just point to libc (`malloc`, `calloc`, `realloc`, etc).

#### Overriding

If you have your own allocator or if you expect an abort in out-of-memory cases,
you can configure the used functions in the following way:

```c
valkeyAllocFuncs myfuncs = {
    .mallocFn = my_malloc,
    .callocFn = my_calloc,
    .reallocFn = my_realloc,
    .strdupFn = my_strdup,
    .freeFn = my_free,
};

// Override allocators (function returns current allocators if needed)
valkeyAllocFuncs orig = valkeySetAllocators(&myfuncs);
```

To reset the allocators to their default libc functions simply call:

```c
valkeyResetAllocators();
```
