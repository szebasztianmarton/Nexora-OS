# Release notes

One file per release, named after the version without the `v` of the tag: `docs/release-notes/MAJOR.MINOR.PATCH.md` (for example `0.4.1.md` for the tag `v0.4.1`). This README is not a release; the files next to it are.

Who reads the file:

- the release workflow (`.github/workflows/release.yml`) fails when the file for the tagged version is missing or empty;
- `scripts/release/make-release.sh VERSION` puts it into the `notes` field of the update bundle's `manifest.json` (the text with leading and trailing white space removed, cut to its first 4000 characters), which the console shows in the Settings update panel. A missing file gives empty notes there.

What goes into it: the changes, known limitations ([mvp-4.md](../mvp-4.md#limitations)), tested hardware ([hardware-compatibility.md](../hardware-compatibility.md)), and links to installation and recovery. Keep the part a user should see on the console within the first 4000 characters. The checklist is in [release-process.md](../release-process.md).
