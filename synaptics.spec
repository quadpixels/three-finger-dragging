Summary: The Synaptics touchpad X driver
Name: synaptics
Version: 0.13.6
Release: 1
License: GPL
Group: User Interface/X
Source: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildPreReq: XFree86-devel

%description
This is a driver for the Synaptics TouchPad for XOrg/XFree86 4.x. A Synaptics
touchpad by default operates in compatibility mode by emulating a standard
mouse. However, by using a dedicated driver, more advanced features of the
touchpad becomes available.

Features:

    * Movement with adjustable, non-linear acceleration and speed.
    * Button events through short touching of the touchpad.
    * Double-Button events through double short touching of the touchpad.
    * Dragging through short touching and holding down the finger on the touchpad.
    * Middle and right button events on the upper and lower corner of the touchpad.
    * Vertical scrolling (button four and five events) through moving the finger on the right side of the touchpad.
    * The up/down button sends button four/five events.
    * Horizontal scrolling (button six and seven events) through moving the finger on the lower side of the touchpad.
    * The multi-buttons send button four/five events, and six/seven events for horizontal scrolling.
    * Adjustable finger detection.
    * Multifinger taps: two finger for middle button and three finger for right button events. (Needs hardware support. Not all models implement this feature.)
    * Run-time configuration using shared memory. This means you can change parameter settings without restarting the X server.

%prep

%setup

%build
make ARCH=%{_arch}

%clean
rm -rf $RPM_BUILD_ROOT

%install

rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
make DESTDIR=$RPM_BUILD_ROOT install

%files
%defattr(-,root,root)
/usr/local
/usr/X11R6
%doc COMPATIBILITY FILES INSTALL INSTALL.DE INSTALL.FR LICENSE README README.alps TODO
%doc trouble-shooting.txt


%changelog
* Tue Feb 03 2004 Giorgio Bellussi <bunga@libero.it>
- Created RedHat compatible .spec file
