# The file tree filesystem
This is a fuse filesystem that act as a client for a [tree](https://thefiletree.com).

## Usage
- To mount a tree :
    tftfs [options] <url> <mount-point> [fuse-options]
- To unmount :
    fusermount -u <mount-point>
- To list the process using the file-system :
    fuser -a <mount-point>
- To mount a tree in debug mode :
    make debug && gdb --args tftfs-debug <url> <mount-point> -f

## Dependencies :
- fuse
- libcurl
- glib-2.0
- libjansson
