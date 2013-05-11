Name:       power-manager
Summary:    Power manager
Version:    1.3.23
Release:    9
Group:      framework/system
License:    APLv2
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/power-manager.manifest 
Requires(post): /usr/bin/vconftool
Source1:        power-manager.service
BuildRequires:  cmake
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(device-node)
BuildRequires:  pkgconfig(heynoti)
Requires(post): system-server

%description
Description: Power manager


%prep
%setup -q 

%build
cp %{SOURCE1001} .
%ifnarch %arm
%if 0%{?simulator}
#for emulator
CFLAGS+=" -DTIZEN_EMUL"
%else
#for real device
CFLAGS+=" -DX86"
%endif
export CFLAGS
%endif
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants
install -m 0644 %{SOURCE1} %{buildroot}%{_libdir}/systemd/system/power-manager.service
ln -s ../power-manager.service %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/power-manager.service

mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc3.d/
mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc5.d/
ln -s %{_sysconfdir}/init.d/pmctrl %{buildroot}%{_sysconfdir}/rc.d/rc3.d/S35power-manager
ln -s %{_sysconfdir}/init.d/pmctrl %{buildroot}%{_sysconfdir}/rc.d/rc5.d/S00power-manager

%post
vconftool set -t int memory/pm/state 0 -i
vconftool set -t int memory/pm/battery_timetofull -1 -i
vconftool set -t int memory/pm/battery_timetoempty -1 -i
vconftool set -t int memory/pm/custom_brightness_status 0 -i -g 5000
vconftool set -t bool memory/pm/brt_changed_lpm 0 -i
vconftool set -t int memory/pm/current_brt 60 -i -g 5000
vconftool set -t int memory/pm/sip_status 0 -i -g 5000

heynotitool set system_wakeup
heynotitool set pm_event

mkdir -p /etc/udev/rules.d
if ! [ -L /etc/udev/rules.d/91-power-manager.rules ]; then
        ln -s %{_datadir}/power-manager/udev-rules/91-power-manager.rules /etc/udev/rules.d/91-power-manager.rules
fi

%files
%manifest power-manager.manifest
%{_sysconfdir}/rc.d/init.d/pmctrl
%{_sysconfdir}/rc.d/rc3.d/S35power-manager
%{_sysconfdir}/rc.d/rc5.d/S00power-manager
%{_bindir}/pm_event
%{_bindir}/pmctrl
%{_bindir}/power_manager
%{_libdir}/systemd/system/power-manager.service
%{_libdir}/systemd/system/multi-user.target.wants/power-manager.service
%{_datadir}/power-manager/udev-rules/91-power-manager.rules
