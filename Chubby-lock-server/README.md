# Basics of Consensus and Go

In this assignment, you will implement a basic Chubby-like lock server in the Go programming language, starting from a simple Key-Value store (taken from github.com/otoolep/hraftd) that uses the hashicorp implementation of the Raft consensus protocols. You will talk to the server using a simple HTTP server connected to it, allowing you to use curl -X to send commands to and receive data from the server:

## Starting the cluster server
I have created a simple Procfile setup for the provided sourcecode that you can use to start and stop the server and individual nodes in it. The default implementation simply starts three servers on the server on which you run, with the second and third nodes starting 5 seconds after the first to give time for the server to start.

```bash
	> goreman start 
	...
	18:46:32 hraftd0 | 2018/11/19 18:46:32 hraftd started successfully
	18:46:33 hraftd0 | 2018/11/19 18:46:33 [DEBUG] raft: Votes needed: 1
	18:46:33 hraftd0 | 2018/11/19 18:46:33 [DEBUG] raft: Vote granted from hraftd0 in term 2. Tally: 1
	18:46:33 hraftd0 | 2018/11/19 18:46:33 [INFO] raft: Election won. Tally: 1
	18:46:33 hraftd0 | 2018/11/19 18:46:33 [INFO] raft: Node at :12000 [Leader] entering Leader state
	18:46:37 hraftd0 | [store] 2018/11/19 18:46:37 received join request for remote node hraftd1 at :12001
	18:46:37 hraftd0 | [store] 2018/11/19 18:46:37 received join request for remote node hraftd2 at :12002
	18:46:37 hraftd0 | 2018/11/19 18:46:37 [INFO] raft: Added peer hraftd1, starting replication
	18:46:37 hraftd0 | [store] 2018/11/19 18:46:37 node hraftd1 at :12001 joined successfully
	18:46:37 hraftd0 | 2018/11/19 18:46:37 [INFO] raft: Added peer hraftd2, starting replication
	18:46:37 hraftd0 | [store] 2018/11/19 18:46:37 node hraftd2 at :12002 joined successfully
	18:46:37 hraftd2 | 2018/11/19 18:46:37 hraftd started successfully
	18:46:37 hraftd1 | 2018/11/19 18:46:37 hraftd started successfully
	...
```

You can then use 'kill' to kill individual processes and watch what happens. *Always be sure that you've killed your processes when you finish working on a shared computer!*

## Required Server Commands
  1. SET (or POST): insert a key/value pair into the store.
```bash
    prompt> curl -XSET server:port/key -d '{foo:bar}'
```
  2. GET: retreive a key from the store. Note that GET should use consensus to retreive the value from the store, not use a simple lock as the base implementation does. This avoids returning stale values from the store.
```bash
    prompt> curl -XGET server:port/key/foo
    { "foo":"bar" }
```
  3. DELETE: delete a key/value association from the server.
```
    prompt> curl -XDELETE server:port/key/foo
```
  4. LOCK: Set the (advisory) lock on a key/value pair. Returns 'true' on success (lock acquisition), 'false' on failure (including if the lock is already held.) LOCK on a entry not yet created creates a locked entry with a value of "" associated with it.
```bash
    prompt> curl -XLOCK server:port/key/foo
    { "foo":true }
```
  5. UNLOCK: Release the (advisory) lock on a key/value pair. Returns 'true' on success (lock release), 'false' on failure (including if the lock wasn't locked.) UNLOCK on a entry not yet created simply returns false.
```bash
    prompt> curl -XLOCK server:port/key/foo
    { "foo":true }
```


## Setting up for running the assignment:

To carry out the assignment, I recommend the following step:

### Setup
  1. Install go tools on the computer on which you work; again, you'll need at least version 1.13 for hashicorp Raft and goreman to work properly. Note that "Golang" is generally the right search term to search for information related to the language. On my macintosh, I use MacPorts to install golang. On Linux machines, "apt-cache search golang" or "yum search golang" will find Go tools, depending on if you're running an Ubuntu or Red Hat linux variant, respectively. Directions for accessing a new version of Go on the CS departmental machines are in a section further below.
  2. Learn some of the basics of the Go programming langauge. The language is generally C-like, though with a different variable declaration syntax and some changes that make it both safer and cleaner. That said, the changes take some getting used to. There's no shortage of information on it, and much you'll learn as you simply work on the project.
  3. Install a few different go packages you need to be able to build the provided source code and run it. Assuming the golang toolchain is installed on your computer (you'll need at least Go version 1.9), you can do so by running the following commands to download the packages you need into your $GOPATH directory (generally ~/go/). 
```bash
    prompt> go env init CS587-go
    prompt> go get github.com/hashicorp/raft
    prompt> go get github.com/hashicorp/raft-boltdb
    prompt> go get github.com/mattn/goreman
    prompt> go install github.com/mattn/goreman
```
  4. Add the binary directory for your $GOPATH (by default ~/go/bin) to you path so you can run goreman directly to start little clusters.
  5. Make sure you can run the simple hraftd:
```bash
    prompt> export PATH=$PATH:$(go env GOPATH)/bin
    prompt> goreman start
```

And in another window
```bash
    prompt> curl -XPOST localhost:11000/key -d '{"foo":"bar"}'
    prompt> curl -XGET localhost:11000/key/foo
```
### To get modern Go on CS machines
The CS machines default to a relatively old version of Go. TO get access to a more recent version, perform the following steps:
1. Open a terminal and go into your home directory ("cd")
2. Open the .bashrc file using "vim .bashrc"
3. In the .bashrc file, copy and paste the following at the bottom of the file (if it is not empty):
```
export GOROOT=/nfs/MLClass/go1.19.3/go
export GOPATH=$HOME/GoProjects
export PATH=$GOPATH/bin:$GOROOT/bin:$PATH
```
4. Adjust the ``GoProjects`` in ``GOPATH`` to the directory where you store your projects. Write and close the file. 
5. Create a symbolic link from the .bashrc  to .bash_login using ``ln -s .bashrc .bash_login``
6. Log out of the computer and log back in. Check the version using ``go version`` ... it should now output ``go version go1.19.3 linux/amd64``

### Assignment Steps
  1. Expand the definition of a Store in http/service.go to include the Lock and Unlock functions. An interface in Go is like an interface in Java - it's a set of named functions that a struct implements to be compatible with a generic calling convention. Use the following definition of the Store interface:
```
// Store is the interface Raft-backed key-value stores must implement.
type Store interface {
        // Get returns the value for the given key.
       	Get(key string) (string, error)

       	// Set sets the value for the given key, via distributed consensus.
       	Set(key, value string) error

       	// Delete removes the given key, via distributed consensus.
       	Delete(key string) error

       	// Lock a given key, via the distributed consensus
       	Lock(key string) (bool, error)

       	// Unlock a given key, via the distributed consensus
       	Unlock(key string) (bool, error)

       	// Join joins the node, identitifed by nodeID and reachable at addr, to the cluster.
       	Join(nodeID string, addr string) error
}
```
You will also need to add empty versions of the Lock() and Unlock() routines to the definition of the TestStore in service_test.go so that the built in test harness continues to compile, and more importantly, to the Store definition in store/store.go.

2. Add "SET", "LOCK", and "UNLOCK" entry points into the http server in http/service.go that call the appropriate functions. "SET" should just fall through into "POST" allowing users to use either "SET" or "POST" to set values in your key/value store. Be sure to note how "GET" returns value to the caller. It creates a Go type, marshalls it into a JSON object, and then sends back that object.

3. Make the Lock() and Unlock() functions you added to store.go queue up "lock" and "unlock" commands to Raft, and add code to the switch statement in Apply() function that call applyLock() and applyUnlock(). Apply is called by Raft to modify the store with log entries, so you want to create your own entry points for handling this.

4. Convert your store from just being a map from strings to strings to being a map from strings to pointers to structures (I called mine "Value"). Do this by defining a struct type (e.g. type Value struct {...}) and then making the definition of the map by map[string] *Value. From here, you'll need to figure out how to allocate structures to insert them into the map and change Get and Set to use this new data structure. You'll also need to modify how Snapshots are saved and restored to take into account the new data structure type.

5. Implement applyLock() and applyUnlock(). Note that applyXXX() returns an "interface {}" which is just a generic object in Go. The invocation of raft.Apply() in the various routines in the Store() interface returns an "ApplyFuture", the result of a computation the Raft state machine applied. After checking that this Future didn't return an error (using f.Error()), you can then extract the result returned by your Apply() function by calling f.Response(). Note that the result of this is a generic type and you'll need to explicitly convert it to the type you want. To convert the response into a boolean, you would say "str := f.Response().(bool)". 

  7. Make Get() use consensus instead of just locally locking, reading, and responding from the map. The original approach results in a faster server (since it can be handled locally by the server without invoking consensus), but can also result in stale reads. The general approach here should be similar to how you handled Step 5, though returning a string from applyGet() instead of a boolean.

## Testing
To test your implementation, make sure you can do each of the following things (this is what I will test):
  1. Insert key/value pairs
  1. Query key/value pair, including pairs that have not been added to the store
  1. Lock key/value pairs, including pairs that have not been added to the store
  1. Make sure that locking an already-locked pair returns "false"
  1. Unlock key/value pairs, and that unlocking pairs that don't exist fails
  1. Make sure that GET uses consensus to retrieve its value
