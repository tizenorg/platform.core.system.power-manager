Name:       power-manager
Summary:    Power manager
Version:    1.3.23
Release:    9
Group:      System/Power Management
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: power-manager.manifest 
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
%cmake .

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}/usr/lib/systemd/system/multi-user.target.wants
install -m 0644 %{SOURCE1} %{buildroot}/usr/lib/systemd/system/power-manager.service
ln -s ../power-manager.service %{buildroot}/usr/lib/systemd/system/multi-user.target.wants/power-manager.service

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
%manifest %{name}.manifest
%license LICENSE.APLv2
%{_bindir}/pm_event
%{_bindir}/pmctrl
%{_bindir}/power_manager
/usr/lib/systemd/system/power-manager.service
/usr/lib/systemd/system/multi-user.target.wants/power-manager.service
%{_datadir}/power-manager/udev-rules/91-power-manager.rules
