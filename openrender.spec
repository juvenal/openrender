# This spec file was generated using Kpp
# If you find any problems with this spec file please report
# the error to ian geiser <geiseri@msoe.edu>
Summary:   Open Source Renderer compliant with the RenderMan 3.2 spec from PIXAR.
Name:      openrender
Version:   0.1
Release:   1
Copyright: LGPL
Vendor:    Juvenal A. Silva Jr. <juvenal.jr@uol.com.br>
Url:       %%url%%

Packager:  Juvenal A. Silva Jr. <juvenal.jr@uol.com.br>
Group:     Applications/Graphics
Source:    openrender-0.1.tar.gz
BuildRoot: /var/tmp/openrender-root

%description
Open Source Renderer compliant with the RenderMan 3.2 spec from PIXAR.
RenderMan is a high quality 3D renderer most used on film production on Hollywood.
The 3.2 spec from PIXAR covers the minimum capabilities that a renderer must have.

%prep
%setup
CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" ./configure \
                --prefix=/usr \
                $LOCALFLAGS
%build
# Setup for parallel builds
numprocs=`egrep -c ^cpu[0-9]+ /proc/stat || :`
if [ "$numprocs" = "0" ]; then
  numprocs=1
fi

make -j$numprocs

%install
make install-strip DESTDIR=$RPM_BUILD_ROOT

cd $RPM_BUILD_ROOT
find . -type d | sed '1,2d;s,^\.,\%attr(-\,root\,root) \%dir ,' > $RPM_BUILD_DIR/file.list.openrender
find . -type f | sed 's,^\.,\%attr(-\,root\,root) ,' >> $RPM_BUILD_DIR/file.list.openrender
find . -type l | sed 's,^\.,\%attr(-\,root\,root) ,' >> $RPM_BUILD_DIR/file.list.openrender

%clean
rm -rf $RPM_BUILD_ROOT/*
rm -rf $RPM_BUILD_DIR/openrender
rm -rf ../file.list.openrender


%files -f ../file.list.openrender
