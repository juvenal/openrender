Name: openRender
Summary: 3D renderer Renderman compliant
Version: 2.2.5
Release: 1
License: GPL/LGPL
Group: Applications
Source: %{name}-%{version}.tar.gz


BuildRoot: %{_tmppath}/build-root-%{name}
Packager: cedric PAILLE
Distribution: suse 10
Prefix: /opt/openrender
Url: http://www.cs.utexas.edu/~okan/openRender/openrender.htm
Provides: openRender renderer



%description
openRender is a RenderMan like photorealistic renderer.
It is being developed in the hope that it will be
useful for graphics research and for people who
can not afford a commercial renderer.
openRender is an open source project licensed under
GPL / LGPL.

Don't forget to set these environment variables:
export OPENRENDERHOME=/opt/openrender
export PATH=$PATH:/opt/openrender/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/lib/openrender
in your ".profile" file in the home dir.

%package devel
Summary: openRender development environment
Group: Development/Libraries

%description devel
Install openRender-devel if you need to develop with the openrender's libraries.

%prep
rm -rf $RPM_BUILD_ROOT 
mkdir $RPM_BUILD_ROOT

%setup -q

%build
./configure --prefix=%{prefix} --enable-selfcontained  --enable-static-openexr CXXFLAGS="-g -fno-strict-aliasing -O2" --enable-static-fltk
make

%install
make DESTDIR=$RPM_BUILD_ROOT install-strip

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,0755)
%dir %{prefix}/
%{prefix}/AUTHORS
%{prefix}/COPYING
%{prefix}/LICENSE
%{prefix}/README
%dir %{prefix}/bin
%{prefix}/bin/*
%dir %{prefix}/displays
%{prefix}/displays/*.so
%dir %{prefix}/man
%dir %{prefix}/man/man1
%{prefix}/man/man1/*.1
%dir %{prefix}/html
%{prefix}/html/*.htm
%{prefix}/html/*.jpg
%dir %{prefix}/html/figures
%{prefix}/html/figures/*.*
%dir %{prefix}/html/rayshadow
%{prefix}/html/rayshadow/*
%dir %{prefix}/html/softshadow
%{prefix}/html/softshadow/*.*
%dir %{prefix}/html/running
%{prefix}/html/running/*.*
%dir %{prefix}/lib
%{prefix}/lib/*.so.0
%{prefix}/lib/*.so.0.0.0
%dir %{prefix}/shaders
%{prefix}/shaders/*.sl
%{prefix}/shaders/*.rslo

%files devel
%defattr(-,root,root)
%dir %{prefix}/include
%{prefix}/include/*
%dir %{prefix}/lib
%{prefix}/lib/*.a
%{prefix}/lib/*.la
%{prefix}/lib/*.so

