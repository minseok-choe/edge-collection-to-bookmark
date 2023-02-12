# ColKeeper (edge-collection-to-bookmark)

Collection feature of MS Edge is useful but doesn't have an convinient way to export all at once, expecially if you want to convert them to bookmarks.

Using ColKeeper, you can easily export Edge collections and import them as bookmark in Edge, Chrome and Firefox.


# How to Use / 사용 방법
1. Download executable from ```Release```, or build on your own. (sqlite3 and curl needed to compile, at least C++17)
2. Open powershell or cmd at the folder where the exe locates
3. Type ```.\colkeeper``` or ```colkeeper``` and enter. If you want to download favicons, ```colkeeper -f``` or ```colkeeper -b```.
4. Go to setting page of your browser(Edge, Chrome or Firefox) and import bookmark. Just opening the exported HTML file by double-click won't import bookmarks. You need to go to settings.

# Supported
- Windows
- Enviroment variables when setting path manually with ```-i```, ```-o```
- Multi-profile (```-p "Profile Folder Name"```)
- Favicons (With ```-f``` or ```-b``` - b is a little faster)
- Sort bookmark and folder by filter

# Not yet supported
- linux
- export non-website elements
