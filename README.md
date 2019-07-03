# netpull

Fast local network file transfers.

## What is it?

netpull is a tool that's designed for file transfers over your local network. In particular:

- No compression is performed.
- No encryption is performed.

Neither of these are necessarily set in stone, but they do mean that at the moment, netpull can
perform zero-copy, multithreaded file transfers over a local network.

## Highlights

- Designed for one-time pulls.
- Runs integrity checks on the downloaded files.
- Pretty fast.
- Supports using multiple threads.
- **Supports resumable transfers.** If your connection drops or the server system crashes,
  then you can resume the job.

## Building

You need [Bazel](https://bazel.build/). Just run:

```
$ bazel build //...
```

to build everything. This should leave two binaries in `bazel-bin`: `netpull_server` and
`netpull_client`.

Note that the build may take a bit because this builds vendored copies of Abseil, protobuf,
and BoringSSL. (Well, also a custom one-file wcwidth from Termux, but if that's the compilation
bottleneck then you *really* need a better system.)

## Running the server

```
    -allow_ip_ranges (Allow IP addresses in this range, @private represents any
      private IP); default: ;
    -deny_ip_ranges (Deny IP addresses in this range, @private represents any
      private IP); default: ;
    -port (The default port to serve on); default: 7420;
    -root (The root directory to serve); default: ".";
    -verbose (Be verbose); default: false;
    -workers (The default number of file streaming workers to use); default: 4;
```

In short, you likely want to run something like:

`-workers` specifies the number of workers to use for file streaming or integrity checks. In
general, you can give a pretty high value for this (e.g. 128) as long as your system can take it.

`-root` is the root directory that will be served. Pretty self-explanatory...

`-allow_ip_ranges` specifies the IP address ranges that will be allowed to connect to the
server. You can pass `@private` to mean that any IP addresses on your local network, or you
can pass a specific IP (run `ip -c addr` on the client to find your local IP address). You can
also pass IP ranges (e.g. `127.0.0.0-127.0.10.10`, meaning any IP address falling within that
range), and you can pass multiple IPs or ranges separated by commas.

In general, this is a good bet to start:

```
$ bazel-bin/netpull_server -workers 128 -allow_ip_ranges @private
```

## Running the client

```
    -server (The server to connect to); default: 127.0.0.1:7420;
    -verbose (Be verbose); default: false;
    -workers (The default number of file forwarding workers to use); default: 4;
```

`-workers` carries a similar significance as before. However, **there seem to be some bugs with
some Linux Wi-Fi drivers where too many open connections at once causes the firmware to crash**.
If you try a ton of workers (e.g. 128), and then all of a sudden your transfer stops, and any
pings fail (e.g. `ping 8.8.8.8`), then try to use a lower worker count. (To fix the issue once it
occurs, restart your device via `ip link set MYDEVICE down` and `ip link set MYDEVICE up`, or
just restart your networking system `systemctl restart NetworkManager`).

Of course, you often won't be connecting to localhost, so you can pass your server IP via
`-server`. (If you didn't change the port on the server end from the default of 7420, then you
can omit it here.)

So your command line might look something like:

```
$ bazel-bin/netpull_client -workers 32 -server 192.168.1.74 / Music
```

The server path (/) is relative to the root directory, so here we're pulling the entire
server root's contents and placing it inside the Music directory on the client system.

You can of course do subdirectories:

```
$ bazel-bin/netpull_client ... /remember/nzk005 nzk005
```

which would pull the `remember/nzk005` subdirectory's contents and place them in the nzk005
directory on the client system.

## Resuming jobs

netpull maintains log files on the server (stored in `~/.cache/netpull`) for each job that
runs (each log file is named using the unique job ID), and these log files can be used to resume
jobs that failed or got stuck.

When the client connects to the server, it will print the job ID. In order to resume this job,
just use replace the server path with `@jobid`. For instance, if you want to resume job ID
`b9ccfad9e66a18e7`, run:

```
$ bazel-bin/netpull_client ... @b9ccfad9e66a18e7 nzk005
```

You still have to specify the output directory.

If a job visibly fails or is interrupted, netpull will print this job ID to the screen at the
very end, so it's easy to see. If for some reason you lose your job ID, you can run:

```
$ ls -lt ~/.cache/netpull | less
```

to show all the recent job IDs, starting at the most recent. Then you can figure out which run
you want to resume from there.

## What to do if something fails

If the client says that the connection was interrupted, received 0 bytes, broken pipe, etc.,
then check the server to see if it's printed error logs.

If the client or server crashes, try to get a coredump and stack trace
(`coredumpctl debug netpull_server` or `coredumpctl debug netpull_client`), then file a bug.
You may need to perform a debug build for that to work
(`bazel build //... --compilation_mode=dbg`).
