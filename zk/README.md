# Using Heightmap Visualizer with Zero-k and other Spring RTS games

## Slope gradient color rendering

Please use the included `zk-pathing.ggr` file as a gradient when rendering slope.

It will heighlight areas that are vehicle-pathable (yellow), bot-pathable (blue), and spider-pathable (purple)

You can change the configured color ranges by looking at GIMP .ggr file format specifications. It's not hard at all, and can be edited with a text editor.

Alternatively, I have supplied a .ggr generator file (`gen-ggr.py`). Just edit the variables in the python script for your own needs (BAR-specific values, different scaling amounts, different colors, etc)

To use this gradient file, simply drag it into your GIMP installation's data folder (likely in .config for linux, and APPDATA for windows) under the `gradients/` subdirectory. Remember to put it on the right GIMP version

If you don't see the gradient, make sure you're putting the file in the right place. You may also want to right click the gradient selection dialog and hit "refresh gradients" if you don't want to restart GIMP to see the changes take effect
