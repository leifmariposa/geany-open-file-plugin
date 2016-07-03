Open File Plugin for Geany
=========================

About
-----------

Open File is a plugin for Geany that provides a dialog for quickly open a file from a preconfigured location.
The dialog has a quick search field that filters out and displays only the files that matches the enterad search word. 
(It is not possible to open files from other location than the preconfigured locations.)


Building and Installing
-----------------------

Download the plugin from https://github.com/leifmariposa/geany-open-file-plugin

Then run the following commands:

```bash
$ make
$ sudo make install
```

Using the Plugin
----------------

After having enabled the plugin inside Geany through Geany's plugin manager,
you'll need to setup a keybinding for triggering the Open File dialog. Go to
the preferences, and under the Keybindings tab set the Open File keybinding, e. g. <Primary><Shift>o.

Open plugin preferences and undet Open File tab add the folders that you want to be able to open files from.
It is also possible to enter a file ending filter, e.g. *.txt.

![screenshot](https://github.com/leifmariposa/geany-open-file-plugin/blob/master/screenshots/configure.png?raw=true)

Using the plugin is simple. Press the keybinding that you selected and the dialog will be shown.
Start typing any part of the file name that you want and the list will be filtered, showing only those 
file names that matches. If the desired file is first in the list (at the top) you can just press enter 
to open it, if not use arrow down until it is selected and then press enter to activate it.

![screenshot](https://github.com/leifmariposa/geany-open-file-plugin/blob/master/screenshots/screenshot.png?raw=true)

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
