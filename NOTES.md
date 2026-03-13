### Python C Functions

Functions placed in the PyMethodDef array cannot return void, instead they must
return a `PyObject*`, for a void function, return a `PyTuple_New(0)`

### TODO:

- Rework background color to allow for a 'c' keybind to show the color prompt and set bg color \
- SVG Support
- Loading gif
- Animated Gif playback with timeline at bottom. Pause with space, lframe `,/<`, rframe `./>`
- Dispatch decode based on image metadata not extension
- Animated Webp playback
- Add toggle help screen function
- Add support for reloading from json in program runtime
- Pixel grid support
- Construction lines background or something
- Better zooming
- Overlays system