Summary: The Synaptics touchpad X driver
Name: synaptics
Version: 0.12.3
Release: 1
License: GPL
Group: User Interface/X
Source: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildPreReq: XFree86-devel

%description

This is a driver for the Synaptics TouchPad for XFree86 4.x. A
Synaptics touchpad by default operates in compatibility mode by
emulating a standard mouse. However, by using a dedicated driver, more
advance features of the touchpad becomes available.

%prep

%setup

%build
make

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
%doc COMPATIBILITY FILES INSTALL LICENSE README README.alps TODO


%changelog
* Tue Feb 03 2004 Giorgio Bellussi <bunga@libero.it>
- Created RedHat compatible .spec file
