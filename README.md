# asynctcpclient
An asynchronous version of TCPClient connect for Particle Photon

Currently, the TCPClient `connect()` method is synchronous. This usually isn’t so bad, unless you have Internet connectivity issues in which case it may take 5 seconds or more for `connect()` to return. Again, normally this isn’t an issue unless you have code that runs out of `loop()` that you want to keep running, even when you’re making outgoing TCP connections that might fail. This is a pretty unusual case, but one that can be troublesome if it’s happening to you!

I made a subclass of `TCPClient` that adds new `asyncConnect()` methods. They take a callback function pointer that’s called when the connect completes, successfully or not. It works by having a separate thread that handles the connections transparently in the background.

Some important notes:

I’m not convinced this is the right way to solve the problem. However, changing the way the built-in connect method works in system firmware isn’t ideal either, because the problem goes deep, deep into the code, and the bottom layer is still synchronous, so it’s a pain to change.

This code does not work on the Electron. I think it’s because the TCP stack on the Electron doesn’t like being called from another thread. I didn’t investigate this too deeply, however, since I didn’t need this to run on the Electron.

It worked when I tested it in a bunch of ways, but I still can’t guarantee that calling connect from another thread is safe, even on the Photon. It’s also important that you don’t make any TCPClient method calls on the object between the time you call asyncConnect and the callback is called for thread-safety reasons.

Make sure you call `AsyncTCPClient::setup()` from your `setup()` method. Also call `AsyncTCPClient::loop()` from `loop()`. The latter is important because the connection callbacks are called on the user/loop thread to make writing your callbacks significantly easier.

I didn’t make it a library because this is more of a proof-of-concept than an actual thing people should use, but it does appear to work, at least on the Photon.
