%define __jar_repack %{nil}
%define _unpackaged_files_terminate_build 0 
%define _enable_debug_packages 0
%global debug_package %{nil}
%global __brp_check_rpaths %{nil}

%define version %(curl -s https://api.github.com/repos/jawaidbazyar2/gssquared/releases/latest | jq -r '.tag_name' | cut -b2-)

Name:           gssquared
Version:        %{version}

Release:        1%{?dist}
Summary:        GSSquared - A Complete Apple II Series Emulator.

License:        GPL3
URL:            https://github.com/jawaidbazyar2/gssquared
Source0:        %{name}.tgz
#Patch0:        %{name}-CMakeLists.patch
Vendor:         Jawaid Bazyar
Packager:       Rickard Osser <rickard.osser@bluapp.com>
Group:          Application/Emulator

BuildRequires:  gcc, gcc-c++, cmake, SDL3-devel, SDL3_image-devel, SDL3_ttf-devel
Requires: SDL3, SDL3_image, SDL3_ttf
%define PREFIX %{buildroot}/usr

%description
GSSquared is a complete emulator for the Apple II series of computers. It is written in C++ and runs on Windows, Linux, and macOS.

%prep

%setup -n %{name}
cd ..
#git clone https://github.com/jawaidbazyar2/gssquared.git -b v%{version}
git clone --recurse-submodules https://github.com/jawaidbazyar2/gssquared.git -b v%{version}

cd gssquared
#%patch 0

%build
#cmake -DCMAKE_BUILD_TYPE=Release -DGS2_PROGRAM_FILES=OFF -DCMAKE_INSTALL_PREFIX=%{_prefix} -S . -B build
cmake -DCMAKE_BUILD_TYPE=Release -DGS2_PROGRAM_FILES=OFF -S . -B build
#cmake --build build
cmake --build build --target package


%install
mkdir -p $RPM_BUILD_ROOT/usr
tar xfz build/gssquared-*-Linux.tar.gz -C $RPM_BUILD_ROOT
mv $RPM_BUILD_ROOT/gssquared*/* $RPM_BUILD_ROOT/usr
rm -rf $RPM_BUILD_ROOT/gssquared*
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/gssquared
#cp -a Docs $RPM_BUILD_ROOT/usr/share/doc/gssquared/
#install -m 0644 README.md $RPM_BUILD_ROOT/usr/share/doc/gssquared


%files
#%license add-license-file-here
%doc README.md Docs
%{_bindir}/GSSquared
/usr/share/applications/GSSquared.desktop
/usr/share/GSSquared
/usr/share/icons/hicolor/scalable/apps/GSSquared.svg


%changelog
* Sun Dec 07 2025 Rickard Osser <ricky@osser.se>
- Initial SPEC-build
