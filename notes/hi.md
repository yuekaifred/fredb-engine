these are notes to store my personal learnings about databases as i design this project.
they can also serve as poor documentation for the insane who want to try and use this.

what this project is (not comprehensive):
a kv value store

what this project is not (not comprehensive (obviously)):
sql compatible

lets start in the beginning. just a hashmap.
okay hashmaps are fast, but runs into the obvious issue of being stored in the memory. memory is very expensive compared to disk space. if we want crash recovery, we would have to write the entire hashmap to disk anyways. _disk is durable, memory is volatile_.

so what if we store the values in the disk, with an in memory hashmap storing a byte offset (location) in disk for the value of every key? that keeps the bulk of our data in the disk and, allows us to access our data in an o(1) disk seek and be much more memory efficient.
good job!

now the problem. updating a value would require a seek. _disks hate seeks. they prefer sequential writes_. 
also, what happens if we have a crash mid write? it would be helpful to have a log of everything that happened before.

introducing, _the append only log: a structure that stores every database transaction in a log._
our in memory hashmap now stores a key and a byte offset to the most recent transaction. transactions in this case are simply writes. deletes append a _tombstone: a transaction telling us that the value has been deleted_.

wow what a roundabout way of doing things! wont this be super expensive on disk space
yes, but we can clean the disk using _compaction_ which deletes old logs and keeps only the most recent key. we can do this in old logs in a background thread because filled logs are _immutable_.

cool. now we get new problems!
1. our hash table must fit in memory.
2. we cant efficiently query a whole range.
3. our compaction algorithm can be slow and memory heavy.

so then some geniuses came together and thought "what if we keep all our segments sorted?" tada! the _SSTable (sorted string table)_ is invented! that addresses all of the previous problems and creates some more. it becomes more intuitive the more you think about it.
because we want to confuse people we also call this structure an _LSM-tree (log-structured merge-tree)_. usually, this refers to the whole system and SSTable refers only to the segments

what we gain:
1. we can eliminate an in memory map of keys to their byte offsets. we now only need to keep a _sparse index_ of SOME keys; the sorted property of our logs allows us to binary search through them in log time. we keep a single sparse index for each segment. if a key isnt in one segment, we look in the previous one.
2. efficient range queries: we can just go in order
3. easier compaction: since logs are sorted we can just take the last log

but there is the obvious question of how can it efficiently keep reverse order.
introducing the _memtable_! it is simply an in memory representation of all our keys for our current segment in a balanced tree. when it becomes too large we throw all that in a file (in sorted order). this can run in the background and we can operate on a new memtable while this is going on.

we want our trees to be balanced because they elegantly handle the worst case of binary trees in logn time; binary trees can promise an average logn operation time but we dont live in a perfect world where humanity is at eternal peace and evil and envy and vancouver doesnt exist and so they can unfortunately fall into n time. 

TODO expand about _red black trees_

cool! we can notice at this point that searching across multiple SSTables is linear. "is my key in here?. no. what about the next one over? no. what about that one? no." and so on and so forth. _disk seeks are sloooooooooowwwwwwwwww_. we want to minimize them as much as possible.

this storage engine is going to ~~plagiarize~~ take inspiration from leveldb. it splits groups of sstables across _levels_.
we eliminate this linear search by maintaining segments in non overlapping groups, allowing for binary search. yay!
first we have the _young level (L0)_. its where we throw all our sstables. we have some allowance for overlaps here, so we keep it small. if that gets too full we compact and ascend that up to another level L1.
any level beyond 0 does not allow overlaps. we maintain this by finding the segments above that overlap with our current level and compacting all those to create new sequence of non overlapping and sorted files in next level. this pattern holds for any level above 0.
leveldb uses a limit of 10^L MB per level. 

TODO make lock more granular
i anticipate this will lead us into MVCC territory!

okay to recap our process of fetching a key:
1. we check the memtable.
2. we check all the SST in L0.
3. we pick one file in L1, then check if key exists in there.
4. we try L2.
5. hmm we try L3.
6. hmm we try L4.
7. hmm we try L5.
8. hmm we try L6.
9. heat death of universe.

so the problem was that they key just didnt exist at all! if only there was a way to predict _if a key exists in our db_. introducing _bloom filters!_

we simply hash the key somewhere and stick it in an array. this gives us o(1) check (assuming constant hash logic) to get a simple yes or no to if the key exist in db by checking its corresponding index. we can get false positive, _never_ false negative.
