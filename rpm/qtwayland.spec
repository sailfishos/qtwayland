Name:       qt5-qtwayland
Summary:    Qt Wayland
Version:    5.9.5
Release:    1%{?dist}
Group:      Qt/Qt
License:    LGPLv3
URL:        https://www.qt.io
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  qt5-qmake >= 5.9.5
BuildRequires:  pkgconfig(Qt5Core) >= 5.9.5
BuildRequires:  qt5-qtfontdatabasesupport-devel
BuildRequires:  qt5-qteglsupport-devel
BuildRequires:  qt5-qteventdispatchersupport-devel
BuildRequires:  qt5-qtthemesupport-devel
BuildRequires:  qt5-qtservicesupport-devel
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(wayland-server) >= 1.6.0
BuildRequires:  pkgconfig(wayland-client) >= 1.6.0
BuildRequires:  pkgconfig(wayland-egl)
BuildRequires:  qt5-qtgui-devel

BuildRequires:  libxkbcommon-devel
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  libffi-devel
BuildRequires:  fdupes

Requires:       %{name}-client = %{version}-%{release}
Requires:       %{name}-compositor = %{version}-%{release}

%description
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.

This package contains the QtWayland client and compositor libraries

%package devel
Summary:        Development files for QtWayland
Group:          Qt/Qt
Requires:       %{name}-client-devel = %{version}-%{release}
Requires:       %{name}-compositor-devel = %{version}-%{release}

%description devel
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.

This package contains the Qt wayland development files.

%package client
Summary:            The QtWaylandClient library
Requires:           xkeyboard-config
Requires(post):     /sbin/ldconfig
Requires(postun):   /sbin/ldconfig

%description client
This package contains the QtWaylandClient library.

%package client-devel
Summary:    Development files for QtWaylandClient
Requires:   %{name}-client = %{version}-%{release}
Requires:   %{name}-tool-qtwaylandscanner = %{version}-%{release}

%description client-devel
This package contains the files necessary to develop
applications that use QtWaylandClient

%package compositor
Summary:            The QtWaylandCompositor library
Requires:           xkeyboard-config
Requires(post):     /sbin/ldconfig
Requires(postun):   /sbin/ldconfig

%description compositor
This package contains the QtWaylandCompositor library.

%package compositor-devel
Summary:    Development files for QtWaylandCompositor
Requires:   %{name}-compositor = %{version}-%{release}
Requires:   %{name}-tool-qtwaylandscanner = %{version}-%{release}

%description compositor-devel
This package contains the files necessary to develop
applications that use QtWaylandCompositor

%package tool-qtwaylandscanner
Summary:    The qtwaylandscanner development tool

%description tool-qtwaylandscanner
%{summary}.

%package plugin-platform-egl
Summary:    The EGL QtWayland platform plugin

%description plugin-platform-egl
%{summary}.

%package plugin-platform-generic
Summary:    The generic QtWayland platform plugin

%description plugin-platform-generic
%{summary}.

%package plugin-graphics-integration-client-drm-egl
Summary:    DRM EGL client graphics integration plugin for QtWayland

%description plugin-graphics-integration-client-drm-egl
%{summary}.

%package plugin-graphics-integration-client-qt-egl
Summary:    Qt EGL plugin client graphics integration plugin for QtWayland

%description plugin-graphics-integration-client-qt-egl
%{summary}.

%package plugin-graphics-integration-server-drm-egl
Summary:    DRM EGL server graphics integration plugin for QtWayland

%description plugin-graphics-integration-server-drm-egl
%{summary}.

%package plugin-graphics-integration-server-wayland-egl
Summary:    Wayland EGL server graphics integration plugin for QtWayland

%description plugin-graphics-integration-server-wayland-egl
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build
export QTDIR=/usr/share/qt5
touch .git
%qmake5

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake_install

# Fix wrong path in pkgconfig files
find %{buildroot}%{_libdir}/pkgconfig -type f -name '*.pc' \
-exec perl -pi -e "s, -L%{_builddir}/?\S+,,g" {} \;
# Fix wrong path in prl files
find %{buildroot}%{_libdir} -type f -name '*.prl' \
-exec sed -i -e "/^QMAKE_PRL_BUILD_DIR/d;s/\(QMAKE_PRL_LIBS =\).*/\1/" {} \;

%fdupes %{buildroot}/%{_includedir}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%exclude %{_libdir}/qt5/plugins/wayland-shell-integration/libivi-shell.so

%files devel
%defattr(-,root,root,-)
%exclude %{_libdir}/cmake/Qt5Gui

%files client
%defattr(-,root,root,-)
%{_libdir}/libQt5WaylandClient.so.*

%files client-devel
%{_includedir}/qt5/QtWaylandClient
%exclude %{_libdir}/libQt5WaylandClient.la
%{_libdir}/libQt5WaylandClient.so
%{_libdir}/libQt5WaylandClient.prl
%{_libdir}/pkgconfig/Qt5WaylandClient.pc
%{_libdir}/cmake/Qt5WaylandClient/*
%{_datadir}/qt5/mkspecs/modules/qt_lib_waylandclient.pri
%{_datadir}/qt5/mkspecs/modules/qt_lib_waylandclient_private.pri

%files compositor
%defattr(-,root,root,-)
%{_libdir}/libQt5WaylandCompositor.so.*

%files compositor-devel
%defattr(-,root,root,-)
%{_includedir}/qt5/QtWaylandCompositor
%exclude %{_libdir}/libQt5WaylandCompositor.so
%{_libdir}/libQt5WaylandCompositor.so
%{_libdir}/libQt5WaylandCompositor.prl
%{_libdir}/pkgconfig/Qt5WaylandCompositor.pc
%{_datadir}/qt5/mkspecs/modules/qt_lib_waylandcompositor.pri
%{_datadir}/qt5/mkspecs/modules/qt_lib_waylandcompositor_private.pri

%files tool-qtwaylandscanner
%defattr(-,root,root,-)
%{_libdir}/qt5/bin/qtwaylandscanner

%files plugin-platform-egl
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/platforms/libqwayland-egl.so

%files plugin-platform-generic
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/platforms/libqwayland-generic.so

%files plugin-graphics-integration-client-drm-egl
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/wayland-graphics-integration-client/libdrm-egl-server.so

%files plugin-graphics-integration-client-qt-egl
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so

%files plugin-graphics-integration-server-drm-egl
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/wayland-graphics-integration-server/libdrm-egl-server.so

%files plugin-graphics-integration-server-wayland-egl
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/wayland-graphics-integration-server/libwayland-egl.so
