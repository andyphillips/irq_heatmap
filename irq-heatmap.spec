Name: 		irq-heatmap
%define         version    1.0
Version:        %version
Release:        1%{?dist}
Summary: 	irq-heatmap
BuildArch: 	x86_64
Group:   	atp/misc
License:    	GPL 
Source0:   	irq-heatmap-%version.tar.gz
BuildRoot: 	/tmp/rpmbuild
Requires:	numactl
BuildRequires:  kernel-devel

%description
This command line tool makes heatmaps of multiple linux stats exposed via /proc in a processor topology
aware way, allowing the visualisation of several parts of the network stack simultaneously. 

%prep
%setup -q

%build
%configure
make %{?_smp_mflags} 

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT 

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%attr(755, root, root) /opt/irq-heatmap/bin/irq_heatmap
%attr(755, root, root) /etc/profile.d/irq-heatmap.sh

%doc

%changelog

%post
