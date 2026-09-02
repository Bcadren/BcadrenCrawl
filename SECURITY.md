# Security Policy for DCSS

Last updated Apr 2020.

## Supported Versions

Generally, we fully support the current unstable version, and the most recent
stable version. At time of writing, this is:

| Version | Supported          |
| ------- | ------------------ |
| 0.25-a  | :white_check_mark: |
| 0.24.1  | :white_check_mark: |
| <0.24.1 | :x:                |

We may, if the vulnerability is severe and affects online play, attempt to
patch earlier versions. Versions before around 0.14 do not reliably build any
more and may be impossible to patch regardless of severity.

Online servers generally run a webtiles server version drawn from trunk, even
if they allow play on older versions of dcss, so any vulnerability in an
up-to-date webtiles server is covered (e.g. in the python code).

## BcadrenCrawl Addendum

BcadrenCrawl keeps up-to-date with mainline's security patches and is as of writing,
a single unstable version with no stable releases. This single version is actively 
supported by Bcadren in the event of a security issue.

As the primary live versions of BcadrenCrawl are similarly hosted on the same servers 
as mainline, mainline's webtiles protections often apply even before being merged into 
the fork.

## Reporting a Vulnerability

It is rather unlikely that there would be a fork-unique security issue; but in case 
that happens, please report the issue to Bcadren directly; either by opening an 
issue on this repository or contacting via bhylton7@gmail.com or /u/Bcadren on reddit.

Additionally, when reporting any security issue discovered in BcadrenCrawl to mainline 
devs, note that it was discovered in this fork; in case it is specific to this fork
and therefore none of their concern.

## Mainline's Reporting instructions

Open an issue on github in this repository. If you would prefer to report the 
issue in private, we recommend either contacting one of the currently active 
devs directly (e.g. via an IRC private message), or sending an email to 
[security@dcss.io] with a subject line including the phrase `dcss security 
report`. Currently this email forwards to @rawlins / advil, who will send an 
acknowledgement and report the issue to the devteam more generally.

If you have access (devteam and server owners) you can directly report a
security issue in private by opening an issue in the https://github.com/crawl/dcss-security
repository.
