# epoll work queue

A simple distributed work queue built with the epoll API.

Can be tested by building the `coordinator` and `worker` CMake targets, then running `./runTest.sh data/urldata.csv`.

The queue can tolerate failures of individual workers by reassigning the tasks to healthy ones.