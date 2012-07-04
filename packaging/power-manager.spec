Name:           power-manager
Version:        1.3.21
Release:        1
License:        Flora License 1.0
Summary:        Tizen Power manager
Group:          System/Power Manager
Source0:        %{name}-%{version}.tar.gz
Source1001:     power-manager.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(devman_plugin)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(vconf)
Requires(post): /usr/bin/vconftool

%description
Tizen Power manager

%prep
%setup -q


%build
cp %{SOURCE1001} .
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?_smp_mflags}

%install
%make_install

mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc3.d/
mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc5.d/

ln -s %{_sysconfdir}/init.d/power_manager.sh %{buildroot}%{_sysconfdir}/rc.d/rc3.d/S35power-manager
ln -s %{_sysconfdir}/init.d/power_manager.sh %{buildroot}%{_sysconfdir}/rc.d/rc5.d/S00power-manager

%post
vconftool set -t int memory/pwrmgr/state 0 -i
heynotitool set system_wakeup

mkdir -p /etc/udev/rules.d
if ! [ -L /etc/udev/rules.d/91-power-manager.rules ]; then
        ln -s /usr/share/power-manager/udev-rules/91-power-manager.rules /etc/udev/rules.d/91-power-manager.rules
fi

%files
%manifest power-manager.manifest
%{_sysconfdir}/rc.d/init.d/power_manager.sh
%{_sysconfdir}/rc.d/rc3.d/S35power-manager
%{_sysconfdir}/rc.d/rc5.d/S00power-manager
%{_bindir}/pm_event
%{_bindir}/pmctrl
%{_bindir}/power_manager
%{_datadir}/power-manager/udev-rules/91-power-manager.rules

