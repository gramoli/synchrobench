Implementation of Citrus: a concurrent RCU search tree based on the paper 
"Concurrent Updates with RCU: Search Tree as an Example" by 
Maya Arbel and Hagit Attiya (Technion).

*Userspace RCU:
    Two implementations are available:  
    1. Our implementation based on epoch-based reclamation. (default)
    2. Userspace library, see <https://lttng.org/urcu/>. 
    
    We suggest trying both versions as performance can vary. 
    In order to run Citrus with userspace library download the library and compile with -DEXTERNAL_RCU. 

*Correct Usage:
    1. Initialize the tree by calling init(). 
    2. Initialize RCU by calling initURCU(int num_threads).
    3. Before calling any of insert/delete/contains, each thread must call urcu_register(int id).
       id should be an integer from the range {0,..., num_threads-1}. 
    4. Run with a scalable memory allocator, for example jemalloc <www.canonware.com/jemalloc/>.

Copyright 2014 Maya Arbel (mayaarl [at] cs [dot] technion [dot] ac [dot] il).

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>