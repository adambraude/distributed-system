# Distributed System
## Consistent hashing
An algorithm for mapping resources to caches in a distributed system. There are two implementations of this algorithm, Ring (devised by Karger et. al), and Jump (by Lamping and Veach).
### Ring
#### Dependencies
You need OpenSSL on your machine in order to use this. In Ubuntu:
```
sudo apt-get install libssl-dev
```
## Two-phase bit vector commit
Using the two-phase commit protocol, the master node ensures that all intended receivers of a bit vector agree to commit by asking for a vote to commit or abort. If any vector votes ABORT then the entire operation is aborted. Otherwise (they all replied OK) the master node sends a commit message with the vector, and upon receipt the slave nodes write the vector to a file.
## Dependencies
### `rpcgen`
You can install this in Ubuntu with
```
sudo apt install libc-dev-bin
```
