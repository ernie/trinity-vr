# Third-Party Software

Trinity VR includes the third-party software below. Full license texts ship in
`licenses/` beside the client; in the source tree they live in
`code/thirdparty/licenses/`.

The engine itself derives from Quake III Arena and ioquake3 and is covered by
the GNU General Public License version 2; see `COPYING.txt`.

## Compiled into the client

| Component | Version | License | Text |
|---|---|---|---|
| zlib | 1.3.1 | zlib license | `COPYING.zlib` |
| Zstandard | vendored, `code/libzstd` | BSD 3-clause | `COPYING.zstd` |
| Opus | 1.5.2 | BSD 3-clause | `COPYING.opus` |
| libjpeg (IJG) | 9f | Independent JPEG Group license | `COPYING.libjpeg` |
| libogg | 1.3.6 | BSD 3-clause | `COPYING.libogg` |
| libvorbis | 1.3.7 | BSD 3-clause | `COPYING.libvorbis` |
| opusfile | 0.12 | BSD 3-clause | `COPYING.opusfile` |

libjpeg's license additionally requires that documentation accompanying a
binary distribution state that "this software is based in part on the work of
the Independent JPEG Group". This notice satisfies that.

## Linked statically, fetched at build time

| Component | Version | License | Text |
|---|---|---|---|
| libcurl | 8.15.0, tag `curl-8_15_0` | curl license | `COPYING.curl` |

Built from `https://github.com/curl/curl.git` at that tag, HTTP(S) only, with
Windows Schannel for TLS.

## Shipped as libraries beside the client

| Component | Version | License | Text |
|---|---|---|---|
| SDL2 | 2.32.8 | zlib license | `COPYING.sdl2` |
| OpenAL Soft | 1.25.1 | LGPL v2 | `COPYING.openal-soft` |
| OpenXR Loader | vcpkg | Apache 2.0 | `COPYING.openxr-loader` |
| JsonCpp | vcpkg | MIT | `COPYING.jsoncpp` |

JsonCpp is a dependency of the OpenXR loader rather than a direct one.

OpenAL Soft is dynamically linked, and any compatible OpenAL implementation may
be substituted by pointing the `s_alDriver` console variable at it. That is what
satisfies LGPL v2 section 6 for a binary distribution.

`haptic_library.dll` is contributed to this project rather than third-party
software, so it is covered by the project's own license and is not listed here.

## Provenance

Each text is the upstream file for the exact version vendored. `COPYING.zlib`,
`COPYING.sdl2` and `COPYING.opus` come from the vendored sources; `COPYING.curl`,
`COPYING.jsoncpp` and `COPYING.openxr-loader` from the artifacts the build links
against; the Xiph files from each project's repository at tags `v1.3.6`,
`v1.3.7` and `v0.12`; `COPYING.libjpeg` from the LEGAL ISSUES section of the
IJG's `jpegsrc.v9f.tar.gz`, whose full README is vendored at
`code/thirdparty/jpeg-9f/README` because the IJG license requires it to
accompany any distribution of the source.

`OpenAL64.dll` is the unmodified upstream 1.25.1 release binary; its
corresponding source under LGPL v2 is
<https://github.com/kcat/openal-soft/releases/tag/1.25.1>.
