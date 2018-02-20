# distributed-bitmap-master-node
Functionality designated to the master node should be implemented here.
## Two-phase bit vector commit
Using the two-phase commit protocol, the master node ensures that all intended receivers of a bit vector agree to commit by asking for a vote to commit or abort. If any vector votes ABORT then the entire operation is aborted, and the master node should either try again, or 
