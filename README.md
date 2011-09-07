## enet.js -- node.js wrapper for enet

This is a Node wrapper for the [enet networking library](http://enet.bespin.org/). enet is a simple networking library that provides sequencing and reliability of packets sent over UDP.

## Building

First, install enet. If it's installed in a non-standard location, make a note of the prefix you used, calling it `ENET_PREFIX`.

Then, build the module with `node-waf`:

    node-waf configure --enet-prefix=${ENET_PREFIX}
    node-waf build --enet-prefix=${ENET_PREFIX}

Then you can create a module directory via the makefile:

    make module

You can then copy the directory `node_modules/enet` to your `node_modules` directory, e.g., `~/node_modules`. When running, make sure that `${ENET_PREFIX}/lib` is in your `LD_LIBRARY_PATH`.

## Usage

The library follows the [enet API](http://enet.bespin.org/modules.html) pretty closely. Reading the docs for enet will help.

You can basically do this:

    var bindaddr = new enet.Address('0.0.0.0', 1234);
    var host = new enet.Host(bindaddr, 10); // bind addr, number of peers to allow at once
    host.on('connect', function(peer, data) {
        // Peer connected.
        // data is an integer with out-of-band data
    }).on('disconnect', function(peer, data) {
        // Peer disconnected.
    }).on('message', function(peer, packet, channel, data)
    {
       // Peer sent a message to us in `packet' on `channel'.
    });
    
    host.start_watcher();
    
    var peer = host.connect(new enet.Address('a.b.c.d', 4331));
    var packet = new enet.Packet('my message', enet.Packet.FLAG_RELIABLE);
    peer.send(0, packet); // channel number, packet.
    // Note that enet invalidates a packet when you send it, so `packet'
    // there is no longer valid at this point.

## Caveats

There isn't a lot of error checking in the C++ code right now. Doing something wrong will likely trigger an assertion error.

The enet loop doesn't use socket readability exclusively when calling the event loop: it also uses a timer to fire the I/O loop periodically. This is because (I think) enet requires this mode of operation; we'd like to make this work better at some point.