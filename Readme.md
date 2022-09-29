# SSDBenchmark

A SSD randomly read performance testing tool for windows platform developer. The app sopports performance testing with different number of threads and concurrent IO requests. The testing result shows the IO request latency time and total read bandwidth in different environments, which can faciliate the user to debug and evaluate the overall performance of the SSD.

# Build
The Windows version has been tested with Enterprise editions of Visual Studio 2022, 2019 and 2017. It should work with the Community and Professional editions as well without any changes.

To build the SSDBenchmark solution, you can use the VisualStudio Command Prompt.

```
msbuild.exe SSDBenchmark.sln /t:Build /p:Configuration="Release" /property:Platform="x64"
```

# Usage

You can use the SSDBenchmark with the following command. To ensure the SSDBenchmark tool running properly, you must run the application with administrator privileges.

```
Usage: SSDBenchmark.exe filename size(GB) blockSize(KB) num_jobs testTime batchSize readMethod(0-async 1-icop) threadNumberForSSDRead memoryLock(0-off 1-on)
```


## Parameter
`filename`: The file path of the test file, the SSDBenchmark tool will reandomly read from this file.

`size`: The testfile size in gigabytes. To avoid the influence of SSD cache, the file size should not be set too small.

`blockSize`: The size of block for each read IO request in kilobytes, usually, this parameter is set as 4.

`num_jobs`: The thread number used for SSD testing.

`testTime`: The test time in seconds.

`batchSize`: The number of concurrent IO requests per sending.

`readMethod`: We now only support two read methods, windows asynchronous reading with waiting (0) and IO completion port(1).

`threadNumberForSSDRead`: The number of threads for parallel SSD read. 

`memoryLock`: Associates a virtual address range with the specified file handle. To lock the overlapped range in memory, and avoid frequently page swithcing. 

To lock the overlapped range in memory, you should assign the privilege from the windows policy group first. The details can be referred to <https://learn.microsoft.com/en-us/sql/database-engine/configure-windows/enable-the-lock-pages-in-memory-option-windows?view=sql-server-ver16>.


## Results
`query latency`: The latency of the IO request from submission to completion.

`send latency`: The layency of the IO request from submission to issued by SSD driver.

`wait latency`: The latency of the IO request from issued by SSD to completion.





