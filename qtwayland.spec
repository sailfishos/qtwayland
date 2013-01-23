%define _qtmodule_snapshot_version 0.0-git803.g4323bf663ea131897857ff564943b17e914ccd9b
%define _qtwayland_variant wayland_egl
Name:       qt5-qtwayland-%{_qtwayland_variant}
Summary:    Qt Wayland compositor, %{_qtwayland_variant} variant
Version:    0.0~git803.g4323bf663ea131897857ff564943b17e914ccd9b
Release:    1%{?dist}
Group:      Qt/Qt
License:    LGPLv2.1 with exception or GPLv3
URL:        http://qt.nokia.com
#Source0:    %{name}-%{version}.tar.xz
Source0:    qtwayland-opensource-src-%{_qtmodule_snapshot_version}.tar.xz
Patch0:    force-glib.patch   
Patch1:    fixeglfs.patch
Patch2:    fullscreen.patch
Patch3:    fixnogl.patch
BuildRequires:  qt5-qtcore-devel
BuildRequires:  qt5-qtgui-devel
BuildRequires:  qt5-qtwidgets-devel
BuildRequires:  qt5-qtopengl-devel
BuildRequires:  qt5-qtplatformsupport-devel
BuildRequires:  qt5-qtdeclarative-devel
BuildRequires:  qt5-qtdeclarative-qtquick-devel
BuildRequires:  qt5-qtv8-devel
BuildRequires:  pkgconfig(wayland-client)
%if "%{_qtwayland_variant}" == "wayland_egl"
BuildRequires:  pkgconfig(wayland-egl)
%endif
%if "%{_qtwayland_variant}" == "xcomposite_egl"
BuildRequires:  pkgconfig(xcomposite)
%endif

BuildRequires:  libxkbcommon-devel
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  libffi-devel
BuildRequires:  fdupes

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

#### Build section

%prep
%setup -q -n qtwayland-opensource-src-%{_qtmodule_snapshot_version}
%patch0 -p1
%patch1 -p1 
%patch2 -p1
%patch3 -p1

%build
export QT_WAYLAND_GL_CONFIG=%{_qtwayland_variant}
export QTDIR=/usr/share/qt5

qmake "QT_BUILD_PARTS += examples"

make %{?_smp_flags}

%install
rm -rf %{buildroot}
%qmake_install
# Fix wrong path in pkgconfig files
find %{buildroot}%{_libdir}/pkgconfig -type f -name '*.pc' \
-exec perl -pi -e "s, -L%{_builddir}/?\S+,,g" {} \;
# Fix wrong path in prl files
find %{buildroot}%{_libdir} -type f -name '*.prl' \
-exec sed -i -e "/^QMAKE_PRL_BUILD_DIR/d;s/\(QMAKE_PRL_LIBS =\).*/\1/" {} \;

# We don't need qt5/Qt/
rm -rf %{buildroot}/%{_includedir}/qt5/Qt


%fdupes %{buildroot}/%{_includedir}


#### Pre/Post section

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig


#### File section

%files
%defattr(-,root,root,-)
%{_libdir}/libQt5Compositor.so.5
%{_libdir}/libQt5Compositor.so.5.*
%{_libdir}/qt5/plugins/platforms/libqwayland.so

%files devel
%defattr(-,root,root,-)
%{_libdir}/libQt5Compositor.so
%{_includedir}/qt5/*
%{_libdir}/libQt5Compositor.la
%{_libdir}/libQt5Compositor.prl
%{_libdir}/pkgconfig/Qt5Compositor.pc
%{_libdir}/cmake/Qt5Compositor/*
%{_datadir}/qt5/mkspecs/modules/qt_lib_compositor.pri

%files examples
%defattr(-,root,root,-)
%{_libdir}/qt5/examples/qtwayland/

#### No changelog section, separate $pkg.changes contains the history

