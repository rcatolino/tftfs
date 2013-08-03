# The file tree filesystem
This is a fuse filesystem that act as a client for a [tree](https://thefiletree.com).

## Usage
- To mount a tree :
 $ tftfs [options] <url> <mount-point> [fuse-options]
- To unmount :
 $ fusermount -u <mount-point>
- To list the process using the file-system :
 $ fuser -a <mount-point>

## Dependencies :
- fuse
- libcurl
- glib-2.0
- libjansson
