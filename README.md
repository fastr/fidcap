fidcap
======

A capture application to test that the FSR172X dummy is capturing RAW12 data

There are a couple of different configurations to try.

Working Configurations:

  * Start `fidcap` - the clocks will be wrong
  * Start `fidcap-configure` and the start `fidcap` - the clocks will be correct

Broken Configurations:

  * Remove the call to `ConfigureAllRegisters` from `fidcap`
    * Start `fidcap`

  * Use the old `capture` (which freezes once per second) and quit it
    * Start `fidcap` - it will continue to freeze, quit
    * Start `fidcap-configure` - it will lock the system and emit kernel debug statements

TODO
====

  * Get driver working better
  * Save data to a file
