This illustrates a race condition on [CyanogenMod's ADBD](https://github.com/CyanogenMod/android_system_core/blob/fb0360bc36831cd3d5d1dd19e01ff6e76d4426f4/adb/backup_service.c) (AOSP also).

Compile on linux.

    $ gcc fork_pipe_test.c -o fork_pipe_test -lpthread

As this is a race condition, it might not break on the first try, or second, or third. It 
can, in fact, take a long time to manifest itself. Therefore, run until you get an error:

    $ true; while [ $? -eq 0 ]; do echo -----; ./fork_pipe_test; done

If that doesn't stop, edit fork_pipe_test.c, enable FORCE_SMALL_DELAY, and start over. If
that's still not enough, enable FORCE_BIG_DELAY instead.

Sample run:

    thiago@thiago-laptop:~/fork_pipe_test$ true; while [ $? -eq 0 ]; do echo -----; ./fork_pipe_test; done
    -----
    Subprocess quit.
    All expected data was read.
    File descriptor closed.
    File descriptor closed.
    -----
    Subprocess quit.
    All expected data was read.
    -----
    Subprocess quit.
    All expected data was read.
    File descriptor closed.
    File descriptor closed.
    -----
    Subprocess quit.
    File descriptor closed.
    Read error: 9
    Too few data was read (99907)
    thiago@thiago-laptop:~/fork_pipe_test$ 
