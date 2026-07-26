PKGCTL(1)

# NAME

pkgctl - Zeppe-Lin package transaction orchestrator

# SYNOPSIS

*pkgctl* [*--help* | *--version*]

# DESCRIPTION

*pkgctl* is the Zeppe-Lin package transaction orchestrator.

Version 0.1.0 establishes the clean-room orchestration model but does not enable
package transaction commands. It performs no source inspection, building,
archive inspection, planning, filesystem application, installed-state
publication, lifecycle execution, or recovery.

The executable succeeds only for help and version queries. Other arguments fail
explicitly instead of entering an incomplete transaction path.

# OPTIONS

*--help*, *-h*
	Print usage and the release capability boundary.

*--version*
	Print the pkgctl release version.

# AUTHORITY

pkgctl owns user intent, typed transaction constraints, package-operation graph
ordering, orchestration progress, continuation policy, diagnostics, and recovery
routing.

It does not own package-source interpretation, package building, archive
inspection, one-package transition policy, physical package application, or
canonical installed state. See *pkgctl_orchestration*(7).

# EXIT STATUS

*0*
	Help or version output completed successfully.

*2*
	The command or arguments are unsupported by this release.

# SEE ALSO

*pkgctl_orchestration*(7)
