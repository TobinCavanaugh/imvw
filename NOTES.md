### Python C Functions

Functions placed in the PyMethodDef array cannot return void, instead they must
return a `PyObject*`, for a void function, return a `PyTuple_New(0)`

### TODO:
- FIX: Leaving grid / shader enabled then loading a new file leaves the shader fixed on despite toggles
- Pass rotation to shaders, fix grid shader
- Dispatch decode based on image metadata not extension
- Add support for reloading from json in program runtime
- ImGui : 
- - Image properties and toggling shaders via checkboxes
- - Add toggle help screen function (make this an imgui window)
- Rework background color to allow for a 'c' keybind to show the color prompt and set bg color
- ICO viewing
- Custom vars in json settings with $Var
- SVG Support
- Loading gif
- Animated Gif playback with timeline at bottom. Pause/play with space, lframe `,/<`, rframe `./>`
- Animated Webp playback
- Construction lines background or something
- Overlays system (?)


### TO RUN
Python313 IS NEEDED, bundling this would be great. Two versions, with python and without python would prob be needed

