# Release Management Guide

This guide explains how to create releases and manage the automated build pipeline for the 7-Zip Parallel Compression Library.

## Overview

The project uses GitHub Actions to automatically build and release binaries for all supported platforms. Releases can be created by pushing a version tag or using the manual workflow trigger.

## Creating a Release

### Method 1: Git Tag (Recommended)

This is the standard method for creating releases:

```bash
# 1. Ensure your code is ready and tested
git checkout main
git pull

# 2. Update CHANGELOG.md with release notes
vim CHANGELOG.md

# 3. Commit the changelog
git add CHANGELOG.md
git commit -m "Prepare for v1.0.0 release"
git push

# 4. Create and push the version tag
git tag v1.0.0
git push origin v1.0.0
```

**What happens next:**
1. GitHub Actions detects the tag
2. Builds start automatically for all platforms
3. Each platform builds in parallel (~10-15 minutes total)
4. Release is created with formatted notes
5. All binaries are uploaded to the release

### Method 2: Manual Trigger

For testing or custom releases:

1. Go to the repository on GitHub
2. Click on "Actions" tab
3. Select "Build and Release" workflow
4. Click "Run workflow" button
5. Enter the version number (e.g., `1.0.0`)
6. Click "Run workflow"

This creates a release with tag `v{version}` if it doesn't exist.

## Version Numbering

Follow [Semantic Versioning](https://semver.org/):

- **Major version** (1.0.0 → 2.0.0): Breaking changes
- **Minor version** (1.0.0 → 1.1.0): New features, backward compatible
- **Patch version** (1.0.0 → 1.0.1): Bug fixes, backward compatible

Examples:
- `v1.0.0` - First stable release
- `v1.1.0` - Added new compression method
- `v1.1.1` - Fixed bug in parallel processing
- `v2.0.0` - Changed API (breaking change)

## Release Artifacts

Each release includes binaries for all platforms:

### Linux x86_64
- **File**: `7zip-parallel-linux-x64-{version}.tar.gz`
- **Contents**: Static library, headers, examples, docs
- **Size**: ~2-5 MB

### Windows x64
- **File**: `7zip-parallel-windows-x64-{version}.zip`
- **Contents**: Static library, headers, examples, docs
- **Size**: ~3-6 MB

### macOS x86_64
- **File**: `7zip-parallel-macos-x64-{version}.tar.gz`
- **Contents**: Static library, headers, examples, docs
- **Size**: ~2-5 MB

### macOS ARM64 (M1/M2)
- **File**: `7zip-parallel-macos-arm64-{version}.tar.gz`
- **Contents**: Static library, headers, examples, docs
- **Size**: ~2-5 MB

## Build Pipeline Details

### Stages

1. **Build Jobs** (parallel):
   - Linux x86_64 (Ubuntu latest)
   - Windows x64 (Windows latest)
   - macOS x86_64 (macOS latest)
   - macOS ARM64 (macOS latest, cross-compile)

2. **Packaging**:
   - Copy library, headers, examples
   - Copy documentation (README, CHANGELOG, DOC/)
   - Create compressed archive
   - Upload as artifact

3. **Release Creation**:
   - Download all artifacts
   - Create GitHub Release
   - Upload all binaries
   - Generate release notes

### Build Time

Typical build times (approximate):
- Linux: 3-5 minutes
- Windows: 5-7 minutes
- macOS: 6-8 minutes
- **Total**: 10-15 minutes (parallel)

## Pre-Release Checklist

Before creating a release, ensure:

- [ ] All tests pass on main branch
- [ ] CI builds are green
- [ ] CHANGELOG.md is updated
- [ ] Version number follows semantic versioning
- [ ] Documentation is up to date
- [ ] Examples compile and run
- [ ] README reflects any new features

## Post-Release Steps

After a release is published:

1. **Verify Downloads**:
   - Download each platform package
   - Extract and verify contents
   - Test example programs

2. **Update Documentation**:
   - Add release announcement to README if major release
   - Update any external documentation

3. **Announce**:
   - Post to relevant forums/communities
   - Update project website if applicable
   - Notify users of major changes

## Troubleshooting

### Build Failures

**Issue**: Build fails on one platform
- **Solution**: Check build logs in Actions tab
- Look for specific error messages
- Test locally on that platform if possible
- May need to adjust CMakeLists.txt or source code

**Issue**: Tests fail during build
- **Solution**: Tests are set to `continue-on-error: true`
- Build will complete but may indicate issues
- Review test output in logs
- Fix tests in a patch release if needed

### Release Creation Failures

**Issue**: Release not created
- **Solution**: Check permissions in repository settings
- Workflow needs `contents: write` permission
- Verify all build jobs succeeded

**Issue**: Artifacts missing from release
- **Solution**: Check individual build job logs
- Verify packaging step completed
- Check file paths in workflow

### Tag Issues

**Issue**: Accidentally created wrong tag
```bash
# Delete local tag
git tag -d v1.0.0

# Delete remote tag
git push origin :refs/tags/v1.0.0

# Create correct tag
git tag v1.0.0
git push origin v1.0.0
```

**Issue**: Release exists, want to add more artifacts
- Delete the release on GitHub
- Delete the tag (see above)
- Push tag again to rebuild

## Manual Package Creation

If you need to create a package manually:

### Linux/macOS
```bash
# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=ON
cmake --build . --config Release

# Package
mkdir -p 7zip-parallel-linux-x64-1.0.0/{include,DOC}
cp lib7zip-parallel.a 7zip-parallel-linux-x64-1.0.0/
cp ../CPP/7zip/Compress/ParallelCompress*.h 7zip-parallel-linux-x64-1.0.0/include/
cp ../README.md ../CHANGELOG.md 7zip-parallel-linux-x64-1.0.0/
cp -r ../DOC 7zip-parallel-linux-x64-1.0.0/
tar -czf 7zip-parallel-linux-x64-1.0.0.tar.gz 7zip-parallel-linux-x64-1.0.0
```

### Windows
```powershell
# Build
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Package
New-Item -ItemType Directory -Force -Path "7zip-parallel-windows-x64-1.0.0\include"
Copy-Item "Release\7zip-parallel.lib" "7zip-parallel-windows-x64-1.0.0\"
Copy-Item "..\CPP\7zip\Compress\ParallelCompress*.h" "7zip-parallel-windows-x64-1.0.0\include\"
Copy-Item "..\README.md" "7zip-parallel-windows-x64-1.0.0\"
Copy-Item -Recurse "..\DOC" "7zip-parallel-windows-x64-1.0.0\"
Compress-Archive -Path "7zip-parallel-windows-x64-1.0.0" -DestinationPath "7zip-parallel-windows-x64-1.0.0.zip"
```

## Continuous Integration

The CI workflow runs on every push and PR:
- Builds on multiple platforms and compilers
- Runs test suite (if available)
- Performs code quality checks
- Validates documentation

This ensures code quality before releases.

## Release Notes Template

Release notes are automatically generated from git commits, but you can customize them. Here's a good template:

```markdown
# 7-Zip Parallel Compression Library v1.0.0

## What's New

- New feature description
- Another new feature
- Bug fix description

## Breaking Changes

- List any API changes
- Migration guide if needed

## Bug Fixes

- Fixed issue with...
- Resolved problem where...

## Performance Improvements

- Compression speed improved by X%
- Memory usage reduced by Y%

## Downloads

See assets below for platform-specific binaries.

## Installation

See [README.md](README.md) for installation instructions.

## Full Changelog

See [CHANGELOG.md](CHANGELOG.md) for complete details.
```

## Monitoring Releases

### GitHub Actions
- Check the "Actions" tab for build status
- Each run shows detailed logs for debugging
- Failed runs send notifications (if configured)

### GitHub Releases
- View all releases at: `https://github.com/JUUTC/7zip/releases`
- Each release shows download counts
- Release notes are displayed prominently

### Download Statistics
GitHub provides download statistics for release assets. View in:
- Releases page (hover over download count)
- Insights → Traffic (for overall repo stats)

## Best Practices

1. **Test Before Tagging**:
   - Ensure CI is green on main
   - Run tests locally on multiple platforms
   - Verify examples compile

2. **Version Strategically**:
   - Don't release too frequently (users can't keep up)
   - Don't wait too long (bugs accumulate)
   - Aim for monthly or quarterly releases

3. **Communicate Changes**:
   - Always update CHANGELOG.md
   - Highlight breaking changes clearly
   - Provide migration examples

4. **Support Old Releases**:
   - Keep old releases available
   - Document which versions are supported
   - Consider LTS (Long Term Support) versions

5. **Security Updates**:
   - Release security fixes quickly
   - Use CVE numbers if applicable
   - Clearly mark security releases

## Questions?

- See `.github/workflows/README.md` for workflow details
- Check GitHub Actions documentation
- Open an issue for problems with the release process

---

**Last Updated**: 2026-01-03

For the latest information, see the repository's GitHub Actions workflows.
