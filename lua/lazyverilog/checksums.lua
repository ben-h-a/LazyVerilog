--- Release binary checksum lock file.
---
--- This file is intentionally committed to the plugin instead of downloaded at
--- install time.  The installer treats these SHA-256 values as the known-good
--- list for release assets and refuses to install a binary when the version or
--- platform is missing.
---
--- Release maintainers update this file before creating a release tag:
---
---   1. Build the exact release binaries.
---   2. Compute each binary's SHA-256 digest.
---   3. Add the digests under the release version below.
---   4. Tag the commit that contains both version.lua and these checksums.
---
--- Keep the shape simple because .github/workflows/release.yml validates the
--- built artifacts against this module before uploading them to a release.

return {
	["v1.2.5"] = {
		["linux-x64"] = "77e3403cebb2b9b7addba417c19e9c356f42bd14dbef929ae35dd3f164caced1",
		["linux-arm64"] = "afdca092914e57e0527d3af1b256f824c5d7e4ad5b48e7c95dfc6c05c986367a",
		["linux-x64-static"] = "a14caeca22fbfc673d6c341be2c785fc2b2cdfc51ddb8c249ce14dbd216edaa0",
		["linux-arm64-static"] = "41f9456c0bd0743f71e917661ff5e42b0b5381a5f00fa15e366b3965b9b883f7",
		["darwin-x64"] = "a75a2b0455580b069a3c0adcc18888ed3e7cbe92239a708e98824a03a7a61763",
		["darwin-arm64"] = "923c85f70a1bda3ddfd1fe9b369a9dae13b869cab3814fa0cfee064f20336d86",
	},

	-- Add the next release as another ["vX.Y.Z"] table with the same platform
	-- keys used above.
}
