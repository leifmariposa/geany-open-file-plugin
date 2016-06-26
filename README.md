Goto Function Plugin for Geany
=========================

About
-----------

Goto Function is a plugin for Geany that provides a dialog for quickly going to a function in the current document.


Building and Installing
-----------------------

Download the plugin from https://github.com/leifmariposa/geany-goto-function-plugin

Then run the following commands:

```bash
$ make
$ sudo make install
```

Using the Plugin
----------------

After having enabled the plugin inside Geany through Geany's plugin manager,
you'll need to setup a keybinding for triggering the Goto Function dialog. Go to
the preferences, and under the Keybindings tab set the Goto Function keybinding.

Using the plugin is simple. Press the keybinding that you selected and the dialog will be shown.
Start typing any part of the function name, if the desired function is first in
the list (at the top) you can just press enter to activete it, if not use arrow down until it is
selected and then press enter to activate it.

![screenshot](https://github.com/leifmariposa/geany-goto-function-plugin/blob/master/screenshots/screenshot.png?raw=true)

License
----------------

This plugin is distributed under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 2 of the
License, or (at your option) any later version. You should have received a copy
of the GNU General Public License along with this plugin.  If not, see
<http://www.gnu.org/licenses/>.

Contact
----------------

You can email me at &lt;leifmariposa(at)hotmail(dot)com&gt;


Bug reports and feature requests
----------------

To report a bug or ask for a new feature, please use the tracker
on GitHub: https://github.com/leifmariposa/geany-goto-function-plugin/issues
