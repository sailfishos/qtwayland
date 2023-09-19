Name:       qt5-qtwayland-wayland_egl
Summary:    Qt Wayland compositor, wayland_egl variant
Version:    5.6.3
Release:    1%{?dist}
License:    LGPLv2 with exception or GPLv3 or Qt Commercial
URL:        https://www.qt.io/
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  qt5-qmake >= 5.6.3
BuildRequires:  pkgconfig(Qt5Core) >= 5.6.3
BuildRequires:  pkgconfig(Qt5Gui) >= 5.6.3
BuildRequires:  qt5-qtplatformsupport-devel >= 5.6.3
BuildRequires:  pkgconfig(Qt5Qml) >= 5.6.3
BuildRequires:  pkgconfig(Qt5Quick) >= 5.6.3
BuildRequires:  pkgconfig(Qt5DBus) >= 5.6.3
BuildRequires:  pkgconfig(wayland-server) >= 1.2.0
BuildRequires:  pkgconfig(wayland-client) >= 1.2.0
BuildRequires:  pkgconfig(wayland-egl)
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
This package contains the Qt wayland compositor for wayland_egl

%package devel
Summary:        Qt Wayland compositor - development files for wayland_egl
Requires:       %{name} = %{version}-%{release}

%description devel
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the Qt wayland compositor development files for wayland_egl

%prep
%autosetup -n %{name}-%{version}

%build
export QT_WAYLAND_GL_CONFIG=wayland_egl
touch .git
%qmake5 "CONFIG += wayland-compositor"

%make_build

%install
%qmake_install

rm %{buildroot}%{_qt5_libdir}/cmake/Qt5Gui/Qt5Gui_.cmake

# Fix wrong path in pkgconfig files
find %{buildroot}%{_qt5_libdir}/pkgconfig -type f -name '*.pc' \
-exec perl -pi -e "s, -L%{_builddir}/?\S+,,g" {} \;
# Fix wrong path in prl files
find %{buildroot}%{_qt5_libdir} -type f -name '*.prl' \
-exec sed -i -e "/^QMAKE_PRL_BUILD_DIR/d;s/\(QMAKE_PRL_LIBS =\).*/\1/" {} \;

# We don't need qt5/Qt/
rm -rf %{buildroot}/%{_qt5_includedir}/Qt
rm -f %{buildroot}/%{_qt5_plugindir}/wayland-graphics-integration-server/liblibhybris-egl-server.so
rm -f %{buildroot}/%{_qt5_plugindir}/wayland-graphics-integration-client/liblibhybris-egl-server.so
rm -r %{buildroot}/%{_qt5_plugindir}/wayland-decoration-client/libbradient.so

%fdupes %{buildroot}/%{_includedir}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%license LICENSE.LGPL* LGPL_EXCEPTION.txt
%license LICENSE.GPL* LICENSE.FDL
%{_qt5_libdir}/libQt5Compositor.so.5
%{_qt5_libdir}/libQt5Compositor.so.5.*
%{_qt5_libdir}/libQt5WaylandClient.so.5
%{_qt5_libdir}/libQt5WaylandClient.so.5.*
%{_qt5_plugindir}/platforms/libqwayland-generic.so
%{_qt5_plugindir}/wayland-graphics-integration-client/libdrm-egl-server.so
%{_qt5_plugindir}/wayland-graphics-integration-server/libdrm-egl-server.so
%{_qt5_plugindir}/platforms/libqwayland-egl.so
%{_qt5_plugindir}/wayland-graphics-integration-client/libwayland-egl.so
%{_qt5_plugindir}/wayland-graphics-integration-server/libwayland-egl.so

%files devel
%defattr(-,root,root,-)
%{_qt5_libdir}/libQt5Compositor.so
%{_qt5_includedir}/*
%{_qt5_libdir}/libQt5Compositor.la
%{_qt5_libdir}/libQt5Compositor.prl
%{_qt5_libdir}/pkgconfig/Qt5Compositor.pc
%{_qt5_libdir}/cmake/Qt5Compositor/*
%{_qt5_archdatadir}/mkspecs/modules/qt_lib_waylandclient.pri
%{_qt5_archdatadir}/mkspecs/modules/qt_lib_waylandclient_private.pri
%{_qt5_archdatadir}/mkspecs/modules/qt_lib_compositor.pri
%{_qt5_archdatadir}/mkspecs/modules/qt_lib_compositor_private.pri
%{_qt5_libdir}/libQt5WaylandClient.so
%{_qt5_libdir}/libQt5WaylandClient.la
%{_qt5_libdir}/libQt5WaylandClient.prl
%{_qt5_libdir}/pkgconfig/Qt5WaylandClient.pc
%{_qt5_libdir}/cmake/Qt5WaylandClient/*
%{_qt5_bindir}/qtwaylandscanner

