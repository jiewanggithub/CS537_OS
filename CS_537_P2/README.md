Running Tests
-------------

Running tests for your system call is easy. 

Copy over the ```initial-xv6``` and ```tester``` directories to your home directory.

Place your modified XV6 source in ```initial-xv6/src``` directory.

Just do the following from inside the \`initial-xv6\` directory:

```
prompt> ./test-getlastcat.sh
```
    

If you implemented things correctly, you should get some notification that the tests passed. If not ... The tests assume that xv6 source code is found in the \`src/\` subdirectory. If it's not there, the script will complain. The test script does a one-time clean build of your xv6 source code using a newly generated makefile called \`Makefile.test\`. You can use this when debugging (assuming you ever make mistakes, that is), e.g.:

```
prompt> cd src/         
prompt> make -f Makefile.test qemu-nox`
```
    

You can suppress the repeated building of xv6 in the tests with the \`-s\` flag. This should make repeated testing faster:

    `prompt> ./test-getlastcat.sh -s`
    

The other usual testing flags are also available. See \[the testing README\](https://github.com/remzi-arpacidusseau/ostep-projects/blob/master/tester/README.md) for details.