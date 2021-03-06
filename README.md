# Rhino-Rox

[![Build Status](https://travis-ci.org/ncloudioj/rhino-rox.svg?branch=master)](https://travis-ci.org/ncloudioj/rhino-rox)

Inspired by Redis, Rhino-Rox aims at providing an easy way to serve your in-memory data structure via network.

# Why Rhino-Rox
Yes, you should use Redis whenever possible. However, occasionally there are some infrequently used data structures that you do want to wrap them up and access from different processes or machines. Imaging you've been asked, "Hey, this data structure is great, can I access it from anywhere like Redis?". Rhino-Rox is an attempt to solve that kind of problem. Recently, the redis author started an initiative that intends to add a [loadable module system][6] to Redis. It would be very exciting to see that feature lands to Redis.

Rhino-Rox reuses a number of modules from Redis,for instance, event handling, sds, [Redis protocal][1] etc. It was *not* designed to provide those advanced features like Redis, i.e. clustering, sentinel, PubSub and so forth. By design, it should just serve as a single-node data structure server. That being said, you can use most of existing Redis clients to communicate with Rhino-Rox server without any extra adaption.

# Build and configure
Clone the repo, then `make`, that's it for now! Rhino-Rox also depends on [jemalloc][2], it'll download and install it automatically.

There are also a few options in the configuration file `rhino-rox.ini`.

# Usage
## Start the server
`$ ./rhino-rox`

## Connect to the server
`$ nc localhost 6000`

## Admin
* `info`
* `type key`
* `ping`
* `echo`

## String
* `set key value`
* `get key value`
* `del key`
* `exists key`

## Trie
* `rset user key value`
* `rget user key`
* `rpget user prefix`
* `rkeys user`
* `rvalues user`
* `rgetall user`
* `rexists user`

## Heapq
* `qpush task 1.0 val1`
* `qpush task 2.0 val2`
* `qpush task 0.5 val3`
* `qpeek task`
* `qpop task`
* `qlen task`
* `qpopn task 100`

## Full Text Searchable (fts) Documents with Okapi BM25 ranking
* `dset animals cat "A cat is trolling a lion"`
* `dset animals dog "A naughty dog is chasing a ball"`
* `dget animals cat`
* `dsearch animals "cat lion"`
* `ddel animals cat`
* `dlen animals`

# What Rhino-Rox really is
Rhino-Rox ([Rhinopithecus Roxellana][3]), also known as golden snub-nosed monkey, is an old World monkey in the Colobinae subfamily. Like giant panda, this cute species is also an endangered one, only 8000-15000 are inhabiting mostly in Sichuan, China. (Yes, Sichuan is also the hometown of panda bears).

# Tip of the hat
* [Redis][4]
* [Jemalloc][2]
* [inih][5]

# License
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
[5]: https://github.com/benhoyt/inih
[6]: http://www.antirez.com/news/106
