%define _qtmodule_snapshot_version 0.0-git855.e5601d283c
%define _qtwayland_variant wayland_egl
Name:       qt5-qtwayland-%{_qtwayland_variant}
Summary:    Qt Wayland compositor, %{_qtwayland_variant} variant
Version:    0.0git855.e5601d283c
Release:    1%{?dist}
Group:      Qt/Qt
License:    LGPLv2.1 with exception or GPLv3
URL:        http://qt.nokia.com
Source0:    %{name}-%{version}.tar.bz2
Source100:	precheckin.sh
BuildRequires:  qt5-qmake >= 5.6.2
BuildRequires:  pkgconfig(Qt5Core) >= 5.6.2
BuildRequires:  qt5-qtplatformsupport-devel >= 5.6.2
BuildRequires:  pkgconfig(Qt5Qml) >= 5.6.2
BuildRequires:  pkgconfig(Qt5Quick) >= 5.6.2
BuildRequires:  pkgconfig(Qt5DBus) >= 5.6.2
BuildRequires:  pkgconfig(wayland-server) >= 1.2.0
BuildRequires:  pkgconfig(wayland-client) >= 1.2.0
%if "%{_qtwayland_variant}" == "wayland_egl"
BuildRequires:  pkgconfig(wayland-egl)
%endif
%if "%{_qtwayland_variant}" == "xcomposite_egl"
BuildRequires:  pkgconfig(xcomposite)
%endif

BuildRequires:  qt5-qtgui-devel >= 5.6.2
Requires: qt5-qtgui >= 5.6.2

BuildRequires:  libxkbcommon-devel
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  libffi-devel
BuildRequires:  fdupes

Requires:       xkeyboard-config

%description
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the Qt wayland compositor for %{_qtwayland_variant}

%package devel
Summary:        Qt Wayland compositor - development files for %{_qtwayland_variant}
Group:          Qt/Qt
Requires:       %{name} = %{version}-%{release}

%description devel
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the Qt wayland compositor development files for %{_qtwayland_variant}

%package examples
Summary:        Qt Wayland compositor - examples
Group:          Qt/Qt
Requires:       %{name} = %{version}-%{release}

%description examples
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the Qt wayland compositor examples for %{_qtwayland_variant}

%prep
%setup -q -n %{name}-%{version}

%build
export QTDIR=/usr/share/qt5
export QT_WAYLAND_GL_CONFIG=%{_qtwayland_variant}
touch .git
%qmake5 "QT_BUILD_PARTS += examples" "CONFIG += wayland-compositor" 

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake_install

rm %{buildroot}%{_libdir}/cmake/Qt5Gui/Qt5Gui_.cmake

# Fix wrong path in pkgconfig files
find %{buildroot}%{_libdir}/pkgconfig -type f -name '*.pc' \
-exec perl -pi -e "s, -L%{_builddir}/?\S+,,g" {} \;
# Fix wrong path in prl files
find %{buildroot}%{_libdir} -type f -name '*.prl' \
-exec sed -i -e "/^QMAKE_PRL_BUILD_DIR/d;s/\(QMAKE_PRL_LIBS =\).*/\1/" {} \;

# We don't need qt5/Qt/
rm -rf %{buildroot}/%{_includedir}/qt5/Qt
rm -f %{buildroot}/%{_libdir}/qt5/plugins/wayland-graphics-integration-server/liblibhybris-egl-server.so
rm -f %{buildroot}/%{_libdir}/qt5/plugins/wayland-graphics-integration-client/liblibhybris-egl-server.so
rm -r %{buildroot}/%{_libdir}/qt5/plugins/wayland-decoration-client/libbradient.so

%fdupes %{buildroot}/%{_includedir}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libQt5Compositor.so.5
%{_libdir}/libQt5Compositor.so.5.*
%{_libdir}/libQt5WaylandClient.so.5
%{_libdir}/libQt5WaylandClient.so.5.*
%{_libdir}/qt5/plugins/platforms/libqwayland-generic.so
%{_libdir}/qt5/plugins/wayland-graphics-integration-client/libdrm-egl-server.so
%{_libdir}/qt5/plugins/wayland-graphics-integration-server/libdrm-egl-server.so

%if "%{_qtwayland_variant}" == "wayland_egl"
%{_libdir}/qt5/plugins/platforms/libqwayland-egl.so
%{_libdir}/qt5/plugins/wayland-graphics-integration-client/libwayland-egl.so
%{_libdir}/qt5/plugins/wayland-graphics-integration-server/libwayland-egl.so
%endif

%if "%{_qtwayland_variant}" == "xcomposite_egl"
%{_libdir}/qt5/plugins/platforms/libqwayland-xcomposite-egl.so
%{_libdir}/qt5/plugins/wayland-graphics-integration-client/libxcomposite-egl.so
%{_libdir}/qt5/plugins/wayland-graphics-integration-server/libxcomposite-egl.so
%endif

%if "%{_qtwayland_variant}" == "nogl"
%{_libdir}/qt5/plugins/platforms/libqwayland-nogl.so
%endif

%files devel
%defattr(-,root,root,-)
%{_libdir}/libQt5Compositor.so
%{_includedir}/qt5/*
%{_libdir}/libQt5Compositor.la
%{_libdir}/libQt5Compositor.prl
%{_libdir}/pkgconfig/Qt5Compositor.pc
%{_libdir}/cmake/Qt5Compositor/*
%{_datadir}/qt5/mkspecs/modules/qt_lib_waylandclient.pri
%{_datadir}/qt5/mkspecs/modules/qt_lib_waylandclient_private.pri
%{_datadir}/qt5/mkspecs/modules/qt_lib_compositor.pri
%{_datadir}/qt5/mkspecs/modules/qt_lib_compositor_private.pri
%{_libdir}/libQt5WaylandClient.so
%{_libdir}/libQt5WaylandClient.la
%{_libdir}/libQt5WaylandClient.prl
%{_libdir}/pkgconfig/Qt5WaylandClient.pc
%{_libdir}/cmake/Qt5WaylandClient/*
%{_libdir}/qt5/bin/qtwaylandscanner

%files examples
%defattr(-,root,root,-)
%{_libdir}/qt5/examples/qwindow-compositor
%{_libdir}/qt5/examples/qml-compositor

