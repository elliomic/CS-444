# CS444 Group 13-04 [Liv vitale, Kiarash Teymoury, Michael Elliot]
# Concurrency 4 Part 1

from threading import BoundedSemaphore, Thread
from time import sleep
from random import randint

class Resource:
    def __init__(self):
        self.MAX_CLIENTS = 3
        self.sema = BoundedSemaphore(value = self.MAX_CLIENTS)
        self.locked = False

    def spaceAvailable(self):
        return self.sema._Semaphore__value > 0

    def acquire(self):
        if self.spaceAvailable and not self.locked:
            self.sema.acquire()
            if not self.spaceAvailable():
                self.locked = True;
            
    def release(self):
        self.sema.release()
        if self.sema._Semaphore__value == self.sema._initial_value:
            self.locked = False

    def isLocked(self):
        return self.locked
        

def threadRoutine(id, res):
    # Have threads gain / release access on resource indefinitely
    while(True):

        spaceAvailable = res.spaceAvailable()
        print("[Thread " + str(id) + "] Space available on resource... " + str(spaceAvailable))

        # Acquire lock if space available and not in a locked state (lock release phase)
        if spaceAvailable and not res.isLocked():
            res.acquire()
            if not res.spaceAvailable():
                res.locked = True;
            print("[Thread " + str(id) + "] Lock acquired!")
            # Sleep with lock for a bit...
            sleep(randint(5, 15))
            # Release lock
            res.release()
            print("[Thread " + str(id) + "] Lock released!")
            # Sleep again to prevent immediately acquiring lock
            sleep(5)
        else:
            # No lock available, sleep
            sleep(randint(2, 6))


if __name__ == "__main__":
    res = Resource()
    
    # Initialize threads
    NUM_THREADS = 10
    for i in range(0, NUM_THREADS):
        # Start threads, specifying thread task
        t = Thread(target=threadRoutine, args=(i, res))
        # Daemonize threads to keep alive
        t.setDaemon(True)
        # Start thread task
        t.start()

    # Do some arbitrary list operations to keep programming running until Ctrl+C
    # I'm 100% sure there's a better way to do this...
    while True:
        list = []
        list.append(None)
