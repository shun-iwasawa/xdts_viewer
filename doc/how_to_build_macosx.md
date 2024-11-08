
# Setting Up the Development Environment on macOS

## Necessary Software

- git
- brew
- Xcode
- cmake (3.2.2 or later)
- Qt 5.x (5.15 or later)

## Building on macOS

### Download and install Xcode from Apple

When downloading Xcode, you should use the appropriate version for your OS version.  You can refer to the Version Comparison Table on https://en.wikipedia.org/wiki/Xcode to find out which version you should use.

Apple store usually provides for the most recent macOS version.  For older versions, you will need to go to the Apple Developer site.

After installing the application, you will need to start it in order to complete the installation.

### Install Homebrew from https://brew.sh

Check site for any changes in installation instructions, but they will probably just be this:

1. Open a Terminal window
2. Execute the following statement:
```
$ /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
```

### Install required software using brew

In a Terminal window, execute the following statements:
```
$ brew install cmake qt@5
```

NOTE: This will install the latest version of QT v5.x which may not be compatible with older OS versions.

If you cannot use the most recent version, download the online installer from https://www.qt.io/download and install the appropriate `macOS` version (min 5.9.2). 

### Set up the repository

These steps will put the xdts_viewer repository under /Users/yourlogin/Documents.
```
$ cd ~/Documents   #or where you want to store the repository#
$ git clone https://github.com/opentoonz/xdts_viewer
```

### Configure environment and Build OpenToonz

1. Create the build directory with the following:
```
$ cd ~/Documents/xdts_viewer
$ mkdir build
$ cd build
```

2. Set up build environment

To build from command line, do the following:
```
$ cmake ../sources -DQTDIR='/usr/local/opt/qt@5'  #replace QT path with your installed QT version#
$ make
```
- If you downloaded the QT installer and installed to `/Users/yourlogin/Qt` instead of by using homebrew, your lib path may look something like this: `~/Qt/5.15.2/clang_64` or `~/Qt/5.15.2/clang_32`

To build using Xcode, do the following:
```
$ sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
$ cmake -G Xcode ../sources -B. -DQTDIR='/usr/local/opt/qt@5' -DWITH_TRANSLATION=OFF   #replace QT path with your installed QT version#
```
- Note that the option `-DWITH_TRANSLATION=OFF` is needed to avoid error when using XCode 12+ which does not allow to add the same source to multiple targets.
- Open Xcode app and open project /Users/yourlogin/Documents/xdts_viewer/build/xdts_viewer.xcodeproj
- Change `ALL_BUILD` to `xdts_viewer`
- Start build with: Product -> Build

Side note: If you want the option to build by command line and Xcode, create a separate build directory for each.

### Setting Up the libraries
- Run the following:

```
$ cd build #or build/Release or build/Debug, where xdts_viewer.app has generated#
$ /usr/local/opt/qt@5/bin/macdeployqt xdts_viewer.app -verbose=1 -always-overwrite
```
- The necessary Qt library files should be in the same folder as IwaWarper binary.
    

### Copying the resource folder

- Copy the entire folder `$xdts_viewer/xdts_viewer_resources` to `xdts_viewer.app/Contents/MacOS`. 

### Running the build

- If built using command line, run the following:
```
$ open $xdts_viewer/build/xdts_viewer.app
```

- If built using Xcode, do the following:

    - Open Scheme editor for xdts_viewer: Product -> Scheme -> Edit Scheme
    - Uncheck: Run -> Options -> Document Versions
    - Run in Debug mode: Product -> Run

    - To open with command line or from Finder window, the application is found in `/Users/yourlogin/Documents/xdts_viewer/build/Debug/xdts_viewer.app`
