# GitHub Actions Workflows

This directory contains GitHub Actions workflows for automated building, testing, and releasing of the 7-Zip Parallel Compression Library.

## Workflows

### 1. CI - Build and Test (`ci.yml`)

**Purpose**: Continuous integration testing on every push and pull request.

**Triggers**:
- Push to `main`, `develop`, `feature/**`, `copilot/**` branches
- Pull requests to `main` or `develop` branches

**Jobs**:
- **Linux Build**: Tests with GCC and Clang in Debug and Release modes
- **Windows Build**: Tests x86 and x64 architectures in Debug and Release modes
- **macOS Build**: Tests x86_64 and ARM64 architectures in Debug and Release modes
- **Code Quality**: Checks for TODOs, file permissions, line endings
- **Documentation**: Validates documentation files exist and format

**Status Badge**:
```markdown
![CI Status](https://github.com/JUUTC/7zip/workflows/CI%20-%20Build%20and%20Test/badge.svg)
```

### 2. Build and Release (`build-release.yml`)

**Purpose**: Build release binaries for all platforms and create GitHub releases.

**Triggers**:
- Push tags matching `v*.*.*` (e.g., `v1.0.0`)
- Manual trigger via workflow dispatch (with custom version)

**Jobs**:
- **Build Linux x86_64**: Creates `.tar.gz` package with library, headers, examples, and docs
- **Build Windows x64**: Creates `.zip` package with library, headers, examples, and docs
- **Build macOS**: Creates separate packages for x86_64 and ARM64 (M1/M2)
- **Create Release**: Uploads all artifacts to GitHub Releases with formatted release notes

**Artifacts**:
Each platform package includes:
- Static library (`.a` or `.lib`)
- C/C++ header files (`ParallelCompressAPI.h`, `ParallelCompressor.h`)
- Example programs (`ParallelCompressExample`, `SimpleTest`)
- Documentation (`README.md`, `CHANGELOG.md`, `DOC/` directory)

## Usage

### Creating a Release

#### Method 1: Git Tag (Automatic)
```bash
# Tag the commit
git tag v1.0.0
git push origin v1.0.0

# Workflow automatically triggers and creates release
```

#### Method 2: Manual Trigger
1. Go to the "Actions" tab in GitHub
2. Select "Build and Release" workflow
3. Click "Run workflow"
4. Enter the version number (e.g., `1.0.0`)
5. Click "Run workflow"

The workflow will:
1. Build binaries for all platforms
2. Package them with documentation
3. Create a GitHub Release with version tag
4. Upload all artifacts to the release

### Release Package Structure

#### Linux/macOS (`.tar.gz`)
```
7zip-parallel-linux-x64-1.0.0/
├── lib7zip-parallel.a           # Static library
├── include/
│   ├── ParallelCompressAPI.h    # C API header
│   └── ParallelCompressor.h     # C++ API header
├── ParallelCompressExample       # Example executable
├── SimpleTest                    # Test executable
├── README.md
├── CHANGELOG.md
└── DOC/                          # Documentation
    ├── BUILD_SYSTEM.md
    ├── CROSS_PLATFORM_STRATEGY.md
    ├── AZURE_INTEGRATION.md
    ├── INVESTIGATION_REPORT.md
    └── README.md
```

#### Windows (`.zip`)
```
7zip-parallel-windows-x64-1.0.0/
├── 7zip-parallel.lib             # Static library
├── include/
│   ├── ParallelCompressAPI.h    # C API header
│   └── ParallelCompressor.h     # C++ API header
├── ParallelCompressExample.exe   # Example executable
├── SimpleTest.exe                # Test executable
├── README.md
├── CHANGELOG.md
└── DOC/                          # Documentation
    └── ...
```

### Viewing Build Status

**For Pull Requests**:
Build status is shown automatically in the PR checks section.

**For Commits**:
View build status in the "Actions" tab or commit history.

**For Releases**:
Each release shows which commit it was built from and includes all artifacts.

## CI/CD Pipeline Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Code Push / PR                            │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              CI Workflow (ci.yml)                            │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ Linux Build  │  │Windows Build │  │ macOS Build  │     │
│  │  - GCC       │  │  - x86       │  │  - x86_64    │     │
│  │  - Clang     │  │  - x64       │  │  - ARM64     │     │
│  │  - Debug     │  │  - Debug     │  │  - Debug     │     │
│  │  - Release   │  │  - Release   │  │  - Release   │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐                        │
│  │Code Quality  │  │Documentation │                        │
│  │  - TODOs     │  │  - Markdown  │                        │
│  │  - Perms     │  │  - Structure │                        │
│  └──────────────┘  └──────────────┘                        │
└─────────────────────────────────────────────────────────────┘
                     │
                     ▼
                  Success ✓
```

```
┌─────────────────────────────────────────────────────────────┐
│                   Git Tag Push / Manual                      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│         Release Workflow (build-release.yml)                 │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ Linux x64    │  │ Windows x64  │  │ macOS x64    │     │
│  │  - Build     │  │  - Build     │  │  - Build     │     │
│  │  - Package   │  │  - Package   │  │  - Package   │     │
│  │  - Upload    │  │  - Upload    │  │  - Upload    │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│                     ┌──────────────┐                        │
│                     │ macOS ARM64  │                        │
│                     │  - Build     │                        │
│                     │  - Package   │                        │
│                     │  - Upload    │                        │
│                     └──────────────┘                        │
│                            │                                 │
│                            ▼                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │           Create GitHub Release                  │       │
│  │  - Download all artifacts                        │       │
│  │  - Create release with formatted notes           │       │
│  │  - Attach all platform binaries                  │       │
│  └─────────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
                     │
                     ▼
            GitHub Release Published ✓
```

## Build Matrix

| Platform | Architecture | Compiler | Build Types | Status |
|----------|-------------|----------|-------------|--------|
| Linux | x86_64 | GCC | Debug, Release | ✅ Enabled |
| Linux | x86_64 | Clang | Debug, Release | ✅ Enabled |
| Windows | x86 | MSVC | Debug, Release | ✅ Enabled |
| Windows | x64 | MSVC | Debug, Release | ✅ Enabled |
| macOS | x86_64 | Clang | Debug, Release | ✅ Enabled |
| macOS | ARM64 | Clang | Debug, Release | ✅ Enabled |

## Environment Variables

Workflows use the following secrets and variables:

- `GITHUB_TOKEN`: Automatically provided by GitHub Actions for creating releases
- No additional secrets required for basic operation

## Troubleshooting

### Build Failures

**CMake configuration fails**:
- Check CMakeLists.txt syntax
- Verify all source files exist
- Check compiler availability

**Build errors**:
- Check build logs in Actions tab
- Look for missing dependencies
- Verify platform-specific code

**Test failures**:
- Tests may be skipped with `continue-on-error: true`
- Check test output in logs
- Run tests locally to reproduce

### Release Issues

**Release not created**:
- Ensure tag matches pattern `v*.*.*`
- Check workflow permissions (needs `contents: write`)
- Verify all build jobs succeeded

**Artifacts missing**:
- Check individual build job logs
- Verify file paths in packaging step
- Ensure files exist before packaging

## Maintenance

### Updating Workflows

1. Edit workflow files in `.github/workflows/`
2. Test changes on a feature branch
3. Verify in Actions tab
4. Merge to main branch

### Adding New Platforms

To add a new platform (e.g., Linux ARM64):

1. Add new job in `build-release.yml`:
```yaml
build-linux-arm64:
  name: Build Linux ARM64
  runs-on: ubuntu-latest
  steps:
    # Use cross-compilation or ARM runner
    # Package and upload artifact
```

2. Update `create-release` job dependencies:
```yaml
needs: [build-linux, build-windows, build-macos, build-linux-arm64]
```

3. Add artifact to release files:
```yaml
files: |
  artifacts/linux-arm64-binary/*.tar.gz
```

### Updating Version

Version is automatically extracted from:
- Git tag (for automatic releases)
- Manual input (for workflow_dispatch)

No hardcoded versions in workflows.

## Best Practices

1. **Always test on feature branch first** before merging
2. **Use semantic versioning** for tags (v1.0.0, v1.1.0, v2.0.0)
3. **Keep build times reasonable** (currently ~10-15 minutes total)
4. **Monitor Actions usage** (free tier has limits)
5. **Update documentation** when changing workflows
6. **Test locally first** with CMake before pushing

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [GitHub Actions Billing](https://docs.github.com/en/billing/managing-billing-for-github-actions)
- [Workflow Syntax](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions)
- [Creating Releases](https://docs.github.com/en/repositories/releasing-projects-on-github)
