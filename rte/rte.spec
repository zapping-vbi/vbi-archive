Name:		rte
Summary:	Real Time Software Video/Audio Encoder library
Version:	0.5.4
Release:	1
Copyright:	GPL
Group:		Applications/Multimedia
Url:		http://zapping.sourceforge.net/
Source:		http://prdownloads.sourceforge.net/zapping/%{name}-%{version}.tar.bz2
Packager:	Michael H. Schimek <mschimek@users.sourceforge.net>
Buildroot:	%{_tmppath}/%{name}-%{version}-root
PreReq:		/sbin/install-info
Provides:	rte

%description
Real Time Software Video/Audio Encoder library for the
Zapping TV viewer.

%prep
%setup -q

%build
%configure
make

%install
rm -rf %{buildroot}
%makeinstall
%find_lang %{name}

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr (-, root, root)
%doc AUTHORS BUGS COPYING ChangeLog NEWS README TODO doc/html
%{_libdir}/librte*
%{_includedir}/librte.h

%changelog
* Sat Dec 14 2002 Michael H. Schimek <mschimek@users.sourceforge.net>
- Added %%find_lang for locale support.

* Fri Oct 4 2002 Michael H. Schimek <mschimek@users.sourceforge.net>
- Updated.

* Tue Jun 18 2002 Michael H. Schimek <mschimek@users.sourceforge.net>
- Requires gettext 0.11.2, amended doc list

* Tue Aug 8 2001 Iñaki García Etxebarria <garetxe@users.sourceforge.net>
- Created
