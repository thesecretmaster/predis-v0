# predis

Predis is a redis clone which is intended to be largely backwards compatible with redis. The main difference is that predis aims to allow concurrent access, while redis has decided that the performance hits that come with parallel access are not worth taking.

In it's current state, there are two executables which predis produces for end user consumption: A command line interface, for quick testing (this may become obsolete soon) in `bin/predis` and a TCP server to allow access to predis in `bin/predis-server`. The server can be accessed using the `redis-cli` executable provided by redis. It uses the RESP protocol created by redis.

## Commands

Predis commands currently are not as similar to redis commands as would be ideal, due to the lack of a hashtable implimentation in the project. This means that instead of using string keys to access the data, data must be accessed with indicies.

Each of these commands can return status codes, which will be described in comments above the method name in `predis.c`.

### `store`

The store command takes two arguments. The type of data to be stored, followed by the value of that data. So, to store the integer 4321, you'd use `store int 4321`.

The store command will return an integer, which is the key by which the stored data can be accessed. This index is the index referenced by the `get`, `del`, and `update` commands.

### `get`

As the name implies, the get command gets the data stored at an index. It takes two arguments, the type, and the index. So, building on the example for `set` (assuming set returned `86`), `get int 86` would return `4321`.

### `delete`

Delete is similar to the get command, however it only takes an index. `delete 86` would delete the element at 86.

### `clean`

Clean is a command which takes no arguments. It simply executes a job which frees up all of the deleted elements.
