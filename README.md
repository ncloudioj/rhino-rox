Inspired by Redis, Rhino-Rox aims for providing an easy way to serve your in-memory data structure via network.

# Why Rhino-Rox
Yes, you should use Redis whenever is possible. However, occasionally there are some infrequently used data structures that you do want to wrap them up and access from different processes or machines. Imaging you've been asked, "Hey, this data structure is great, can I access it from anywhere like Redis?". Rhino-Rox is an attempt to solve that kind of problem.

Instead of forking off Redis, Rhino-Rox reuses a number of modules from Redis, for instance, event handling, sds, [Redis protocal](1) etc. It was *not* designed to provide those advanced features like Redis, i.e. clustering, sentinel, PubSub and so forth. By design, it should just serve as a single-node data structure server.

# Target data structures
Honestly, don't know yet. Perhaps something like interval tree, trie, compressed bitmap I guess. Just managed to get it up and running as an echo server. :)

# Build and configure
Clone the repo, then `make`, that's it for now! Rhino-Rox also depends on [jemalloc](2), it'll download and install it automatically.

There are also a few options in the configuration file `rhino-rox.ini`.

# What Rhino-Rox really is
Rhino-Rox ([Rhinopithecus Roxellana](3)), also known as golden snub-nosed monkey, is an Old World monkey in the Colobinae subfamily. Like giant panda, this cute species is also an endangered one, only 8000-15000 are inhabiting mostly in Sichuan, China. (Yes, Sichuan is also the hometown of panda bears).

# Tip of the hat
* [Redis](4)
* [Jemalloc](2)

# Licence
BSD

# Structure of the codebase
The codebase is organized much like Redis, just simplied a bit due to its design goals mentioned above. In a nutshell, equipped by non-blocking IO, Rhino-Rox is a single thread application along with timers excutes periodically during socket IO handling. By and large, there are two major parts: the server and the data structures:

* Server
    * loading configurations (rr_config.c)
    * server initialization, query handling, and close (rr_server.c, rr_reply.c)
    * event loop and network handling (rr_event.c, rr_epoll.c, rr_kqueue.c, rr_network.c)
    * timer and server cron job (rr_event.c, rr_server.c)

* Data structures
    * a simple dynamic array and a heap built upon it (rr_array.c, rr_minheap.c)
    * double linked list, implemented by Redis (adlist.c)
    * will add more to be finally served by Rhino-Rox

[1]: http://redis.io/topics/protocol
[2]: https://github.com/jemalloc/jemalloc
[3]: https://en.wikipedia.org/wiki/Golden_snub-nosed_monkey
[4]: http://redis.io/
