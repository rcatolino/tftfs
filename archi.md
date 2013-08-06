## Modules :
- *tftfs* contains the implementation of the various fuse callbacks, and the main function.
When a fuse callbacks is called it can do the following work :
  1. (de)initialize the long-lived data structures associated with the code
  2. call the related method for tree in the tft module.
  3. translate any potential error into a standard errno, and return it.
- *tft* contains the method equivalent to the fuse callbacks. All these methods do is calling
  one of the 3 tree api function (ls, new, rm) and pass a callback that will interpret the
  response.
  The callbacks parse the json response of the server and fill any fuse data structures they
  might have to fill.
- The *parsing* module contains helper function to parse the json in the answers.
- The *http* module contains the method to initialize the http connections handles and perform
  POST request using them.
- The *connections* module contains methods to manage a pool of http connections.
