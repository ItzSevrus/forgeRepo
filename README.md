# Forge Official Repository

**A curated collection of software for Forge, built around trust, transparency, and user control.**

The Forge Official Repository is the official community-facing package source for [Forge](https://github.com/ItzSevrus/forgeRepo), a lightweight, user-local package manager written in C.

Our goal is simple: provide useful applications that users can discover, inspect, and install without unnecessarily giving up control over their systems or personal data.

We believe a package repository should do more than distribute software. It should take responsibility for what it chooses to publish, explain how packages are delivered, and make its security expectations clear.

---

## Table of Contents

- [Why This Repository Exists](#why-this-repository-exists)
- [Our Trust and Privacy Policy](#our-trust-and-privacy-policy)
- [How Package Installation Works](#how-package-installation-works)
- [Repository Structure](#repository-structure)
- [Getting Started with Forge](#getting-started-with-forge)
- [Creating Your Own Forge Repository](#creating-your-own-forge-repository)
- [Publishing Your First Package](#publishing-your-first-package)
- [Adding and Using a Custom Repository](#adding-and-using-a-custom-repository)
- [Maintaining Your Repository](#maintaining-your-repository)
- [Reporting Security or Privacy Concerns](#reporting-security-or-privacy-concerns)
- [Transparency and Limitations](#transparency-and-limitations)

---

## Why This Repository Exists

Software installation should not require users to blindly trust everything they download.

Many applications need access to files, networks, system resources, or personal information to function. Some collect more data than necessary, obscure their behavior, or introduce unwanted functionality. Users deserve to know what they are installing and which maintainers are responsible for providing it.

The Forge Official Repository exists to offer a **curated, transparent, and privacy-conscious collection of applications** for Forge users.

Our goals are to:

- Provide useful software with clearly identified sources and versions.
- Reject software known to contain malware, malicious behavior, or intentionally harmful functionality.
- Exclude applications that engage in deceptive data collection, invasive tracking, or unjustified privacy violations.
- Review package manifests, build scripts, download sources, and relevant dependencies before accepting packages.
- Use SHA-256 checksums to detect source archives that do not match their published manifests.
- Make package versions and installation behavior understandable.
- Encourage responsible software distribution and security reporting.

The official repository is curated rather than an unrestricted directory where every submitted package is automatically accepted.

**Our policy is not to knowingly publish harmful or privacy-invasive applications.** If credible evidence shows that an accepted package violates this policy, it may be suspended or removed while the issue is investigated.

## Our Trust and Privacy Policy

### 1. Security-conscious package selection

Packages should have a legitimate purpose and a source that can be reviewed. We do not knowingly accept packages intended to steal credentials, distribute malware, damage systems, conduct unauthorized surveillance, or otherwise harm users.

We may reject packages whose behavior is deceptive, whose source cannot be adequately assessed, or whose security risks are disproportionate to their purpose.

### 2. Privacy comes first

Applications that unnecessarily collect personal information, secretly track users, transmit sensitive information without appropriate disclosure, or engage in invasive surveillance are not suitable for the official repository.

Network access or telemetry is not automatically malicious. Where relevant, maintainers should be able to explain what information an application collects, why it is needed, and how it is handled.

Applications with legitimate network functionality may still be accepted when their behavior is sufficiently transparent and their privacy implications are reasonable.

### 3. Package review matters

Review may include examining:

- Package manifests and declared dependencies.
- Source repositories and release archives.
- Build and installation scripts.
- Download URLs and version information.
- Checksum consistency.
- Known security issues and reported privacy concerns.
- Unexpected network access, file modifications, or other suspicious behavior where practical.

The depth of review may vary depending on the package and the resources available.

### 4. No knowingly harmful submissions

Submitting a package does not guarantee acceptance. A package may be rejected, delayed, suspended, or removed if it fails our requirements or new evidence raises credible concerns.

Maintainers should provide accurate package metadata and promptly report significant security or privacy problems.

---

## How Package Installation Works

Forge uses repositories containing package metadata and versioned package directories.

A typical installation works like this:

1. Forge searches its locally synced package catalog.
2. The user selects a package and version.
3. Forge displays package details and warns that the build script will execute with the user's permissions.
4. The user reviews the proposed installation and explicitly approves it.
5. Forge checks declared system dependencies and required build tools.
6. Forge downloads the source archive and build script.
7. Forge verifies the source archive's SHA-256 checksum against the package manifest.
8. Forge executes the declared build command.
9. The package is installed into the configured user-local prefix.

This approach provides visibility into important installation steps and helps users make informed decisions.

A checksum verifies that an archive matches the digest recorded in the manifest. It does not independently establish that the archive is benign. For that reason, source review and repository curation remain important.

---

## Repository Structure

A Forge repository follows a versioned directory layout.

For example:

```text
my-forge-repository/
├── README.md
├── repository.conf
└── packages/
    └── hello-world/
        └── v1.0.0/
            ├── forge.pkg
            └── build.sh
```

| File or directory | Purpose |
|---|---|
| `README.md` | Explains the repository and its policies. |
| `repository.conf` | Describes the repository and its format. |
| `packages/` | Contains package directories. |
| `packages/<name>/<version>/forge.pkg` | Manifest for a particular package version. |
| `packages/<name>/<version>/build.sh` | Build and installation script for that version. |

Each version is maintained independently. This allows multiple releases of the same package to coexist without overwriting their manifests or build scripts.

For a working package example, inspect the [`hello-world` package](https://github.com/ItzSevrus/forgeRepo/tree/master/packages/hello-world).

---

## Getting Started with Forge

First, install or build Forge using the instructions in the [Forge project repository](https://github.com/ItzSevrus/forgeRepo).

If you have already built Forge from source, the binary will typically be available at `build/forge`.

Check that the CLI works:

```sh
forge --help
```

Initialize its configuration if needed:

```sh
forge setup
```

The following commands demonstrate the repository workflow used by the current development version:

```sh
forge repo list
forge repo sync official
forge repo validate
forge search hello
forge info hello-world
```

The `official` alias must already be configured. If it is not, follow the repository-add syntax shown by your installed version's help.

To install the example package:

```sh
forge install hello-world@v1.0.0
```

Review the package details and build script before approving installation.

After a successful installation, the example executable can be run with:

```sh
hello
```

Expected output:

```text
Hello from forge!
```

---

## Creating Your Own Forge Repository

Forge is not limited to the official repository. You can create your own repository for personal tools, organization-specific applications, experimental software, or packages that are not included in the official collection.

Your repository is controlled by you. You are responsible for its contents, package quality, security practices, and maintenance.

### Step 1: Create a Git repository

Create a new empty repository on GitHub or another Git hosting service.

For example, create a repository named `my-forge-repository`, then clone it:

```sh
git clone https://github.com/YOUR_USERNAME/my-forge-repository.git
cd my-forge-repository
```

Replace `YOUR_USERNAME` and the repository name with your own values.

### Step 2: Create `repository.conf`

Create the repository metadata file at the root:

```sh
nano repository.conf
```

Use this example configuration:

```ini
[repository]
name = "my-forge-repository"
format = 2
description = "My personal Forge package repository"
default_branch = "main"
package_layout = "packages///forge.pkg"
```

The fields describe:

- `name`: a human-readable repository identifier.
- `format`: the repository metadata format supported by your Forge version.
- `description`: a short explanation of the repository.
- `default_branch`: the Git branch containing the repository metadata and packages.
- `package_layout`: the versioned manifest layout used by this repository.

**Important:** Use a format version supported by your installed Forge release. The example above follows the format-2 layout used by the current development workflow. If your Forge version expects another format, follow that version's documentation and working repository example.

### Step 3: Create a package directory

Every package should have its own directory, followed by a directory for its version.

For example:

```sh
mkdir -p packages/hello-world/v1.0.0
```

Your directory structure should now look like this:

```text
my-forge-repository/
├── repository.conf
└── packages/
    └── hello-world/
        └── v1.0.0/
```

The version directory keeps the package's metadata and build script associated with that specific release.

### Step 4: Add the package manifest

Create the manifest:

```sh
nano packages/hello-world/v1.0.0/forge.pkg
```

The manifest must follow the schema accepted by your Forge version. It should describe the package identity, version, source archive, SHA-256 checksum, build command, and dependencies as required by that schema.

Do not invent field names or guess the manifest syntax. Copy the structure of the working [`hello-world` manifest in this repository](https://github.com/ItzSevrus/forgeRepo/tree/master/packages/hello-world), then replace its values with those for your package.

The source archive URL and checksum must refer to the same exact archive. Incorrect metadata may cause validation, download, or installation to fail.

### Step 5: Write the build script

Create the build script:

```sh
nano packages/hello-world/v1.0.0/build.sh
```

The build script is responsible for building the package and placing its files into the installation destination expected by Forge.

Use the official example's `build.sh` as your starting point and adapt it to your software. Do not assume that Forge automatically understands every project's build system.

Your script should:

- Build only what the package requires.
- Install files into the destination expected by Forge.
- Avoid changing unrelated files.
- Avoid collecting or transmitting user data without a legitimate, disclosed reason.
- Return a nonzero exit status if a required build or installation step fails.
- Avoid unnecessary administrator privileges.

Make it executable in the repository:

```sh
chmod +x packages/hello-world/v1.0.0/build.sh
```

Even if the executable bit is set in Git, always verify that the manifest's build command matches the script's location and name.

### Step 6: Publish a source archive

Forge needs the source archive specified by the manifest to be accessible at its declared URL.

Build the archive containing the files that your build script expects. For example:

```sh
tar -czf hello-world-1.0.0.tar.gz hello-world-source/
```

Replace `hello-world-source/` with the actual directory containing your source files.

Upload the resulting archive as a release asset on your Git hosting service or another stable download location.

Make sure the archive contains the expected files at the expected paths. A script that refers to `./configure`, for example, will fail if that file is missing or located somewhere else inside the archive.

### Step 7: Calculate the SHA-256 checksum

Calculate the checksum of the exact archive you published:

```sh
sha256sum hello-world-1.0.0.tar.gz
```

Copy the resulting digest into the package manifest's checksum field, using the exact field name and syntax required by Forge.

If you replace or rebuild the archive, calculate the checksum again. Even a small change produces a different digest.

Do not use a checksum from a different version or a different archive.

### Step 8: Test the package

Before publishing, verify the following:

- The manifest follows the supported schema.
- The archive URL is reachable.
- The checksum matches the published archive.
- The archive contains all required source files.
- The build script uses the expected paths and commands.
- Required dependencies are declared accurately.
- The software builds and runs as expected.
- The script does not unexpectedly access, alter, or transmit user data.

Whenever possible, test the package in a disposable user account or isolated environment. A build script runs with the installing user's permissions, so testing untrusted scripts on your main account is risky.

### Step 9: Commit and push the repository

After adding the metadata and package files:

```sh
git add README.md repository.conf packages/
git commit -m "Add hello-world v1.0.0"
git push origin main
```

Ensure that the pushed branch matches the `default_branch` value in `repository.conf`.

At this point, your repository is published. The next step is to add it to Forge.

---

## Adding and Using a Custom Repository

### Step 1: Find the repository-add syntax

Different Forge versions may expose different argument orders or options. Check your installed CLI:

```sh
forge repo --help
```

Use the `repo add` syntax shown by that command to register your repository URL under an alias.

For example, your intended alias might be `personal`, and your repository URL might be:

```text
https://github.com/YOUR_USERNAME/my-forge-repository
```

Choose a short alias that you can recognize later. Replace the placeholders with your real repository details.

### Step 2: Verify that Forge recognizes it

List your configured repositories:

```sh
forge repo list
```

Confirm that the new alias and URL appear.

### Step 3: Synchronize the repository

Fetch and index its catalog:

```sh
forge repo sync personal
```

Replace `personal` with the alias you configured.

If synchronization fails, verify the repository URL, branch, `repository.conf`, and package layout.

### Step 4: Validate the catalog

Run:

```sh
forge repo validate
```

Fix any reported metadata or catalog problems before attempting an installation.

A valid catalog does not guarantee that every package's build script will succeed, so package testing is still necessary.

### Step 5: Search for your package

For the example package:

```sh
forge search hello-world
```

Inspect its details using the supported package-information command:

```sh
forge info hello-world
```

If the package does not appear, check that the versioned directory and manifest are committed to the correct branch and that the repository has been synchronized.

### Step 6: Install the desired version

Use the package's exact name and version:

```sh
forge install hello-world@v1.0.0
```

Review the package details and the build script before approving the installation.

After successful installation, verify that the program runs and behaves as expected.

### Step 7: Update your repository

When you publish a new package version:

1. Create a new version directory.
2. Add its manifest and build script.
3. Publish its source archive.
4. Calculate and record the archive's SHA-256 checksum.
5. Test the package.
6. Commit and push your changes.
7. Synchronize the repository again in Forge.

For example:

```sh
forge repo sync personal
forge repo validate
forge search hello-world
```

Keep old version directories when you intend to continue offering those releases.

---

## Maintaining Your Repository

A package repository requires ongoing maintenance.

Repository maintainers should:

- Keep source URLs and checksums accurate.
- Test new releases before publishing them.
- Review security advisories relevant to published packages.
- Remove or update packages that are abandoned, compromised, or no longer suitable.
- Document known limitations and required dependencies.
- Respond to credible security and privacy reports.
- Avoid changing a published archive without updating its checksum and metadata.
- Make package changes traceable through version control.

For organizations maintaining private or specialized repositories, establish clear rules for who may publish packages and who reviews their changes.

## Reporting Security or Privacy Concerns

If you discover a package that appears malicious, invasive, deceptive, or otherwise unsafe, please report it to the repository maintainers through the repository's [GitHub Issues](https://github.com/ItzSevrus/forgeRepo/issues).

A useful report includes:

- The package name and version.
- The relevant manifest or build-script path.
- A clear explanation of the suspicious behavior.
- Reproduction steps or supporting evidence, where safe to provide.

Do not publish passwords, access tokens, private information, or exploit details that could unnecessarily put users at risk.

Reports will be assessed, and packages may be suspended or removed while concerns are investigated.

## Transparency and Limitations

The official repository's curation policy is a commitment to responsible software distribution, not a claim that every possible threat can be eliminated.

No review process can guarantee that software is free of vulnerabilities, that upstream maintainers have never been compromised, or that an application will remain safe indefinitely. A package can change upstream, develop new behavior, or contain problems that were not detected during review.

SHA-256 verification protects against an archive differing from the recorded digest, but it does not prove that the archive is harmless. Likewise, running Forge without administrator privileges limits the privileges available to its build scripts, but does not sandbox them.

For these reasons, we combine package curation with transparent installation steps, checksum verification, explicit user approval, and a process for reporting concerns.

**Our commitment:** we will not knowingly accept harmful or privacy-invasive software into the official repository, and we will work to investigate and address credible reports when problems are discovered.

Thank you for helping make Forge a more transparent and privacy-conscious software ecosystem.

---

**Forge Official Repository**  
Repository: [github.com/ItzSevrus/forgeRepo](https://github.com/ItzSevrus/forgeRepo)  
Package manager: [Forge](https://github.com/ItzSevrus/forgeRepo)
