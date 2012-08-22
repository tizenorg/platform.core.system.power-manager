Name:       power-manager
Summary:    Power manager
Version:    1.3.21
Release:    1
Group:      TO_BE/FILLED_IN
License:    Flora Software License 
Source0:    %{name}-%{version}.tar.gz
Requires(post): /usr/bin/vconftool
BuildRequires:  cmake
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(devman_plugin)
BuildRequires:  pkgconfig(heynoti)

%description
Description: Power manager


%package bin
Summary:    Power manager
Group:      TO_BE/FILLED_IN

%description bin
Description: Power manager


%prep
%setup -q 

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

%build
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc3.d/
mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc5.d/

ln -s %{_sysconfdir}/init.d/power_manager.sh %{buildroot}%{_sysconfdir}/rc.d/rc3.d/S35power-manager
ln -s %{_sysconfdir}/init.d/power_manager.sh %{buildroot}%{_sysconfdir}/rc.d/rc5.d/S00power-manager

%post bin
vconftool set -t int memory/pm/state 0 -i
heynotitool set system_wakeup

mkdir -p /etc/udev/rules.d
if ! [ -L /etc/udev/rules.d/91-power-manager.rules ]; then
        ln -s /usr/share/power-manager/udev-rules/91-power-manager.rules /etc/udev/rules.d/91-power-manager.rules
fi



%files bin
%defattr(-,root,root,-)
/etc/rc.d/init.d/power_manager.sh
/etc/rc.d/rc3.d/S35power-manager
/etc/rc.d/rc5.d/S00power-manager
/usr/bin/pm_event
/usr/bin/pmctrl
/usr/bin/power_manager
/usr/share/power-manager/udev-rules/91-power-manager.rules

