# GitHub Actions Workflows

Automated CI/CD pipelines for building, testing, and releasing the 7-Zip Parallel Compression Library.

## Workflows

### CI - Build and Test (`ci.yml`)

**Runs on**: Every push and pull request

**Tests**:
- Linux: GCC + Clang (Debug + Release)
- Windows: x86 + x64 (Debug + Release)  
- macOS: x86_64 + ARM64 (Debug + Release)
- Code quality and documentation checks

**Status Badge**:
```markdown
![CI Status](https://github.com/JUUTC/7zip/workflows/CI%20-%20Build%20and%20Test/badge.svg)
```

### Build and Release (`build-release.yml`)

**Triggers**:
- Push tags: `v*.*.*` (e.g., `v1.0.0`)
- Manual: Actions → Build and Release → Run workflow

**Builds**:
- Linux x86_64 (`.tar.gz`)
- Windows x64 (`.zip`)
- macOS x86_64 + ARM64 (`.tar.gz`)

**⚡ All binaries include parallel compression support.**

Each package contains: library, headers, examples, documentation

---

## Creating a Release

### Tag-based (Recommended)
```bash
git tag v1.0.0
git push origin v1.0.0
```

Workflow automatically builds all platforms and publishes release.

### Manual Trigger
1. Go to Actions → "Build and Release"
2. Click "Run workflow"
3. Enter version number (e.g., `1.0.0`)

---

## Release Package Structure

```
7zip-parallel-{platform}-{version}/
├── lib7zip-parallel.a (or .lib on Windows)
├── include/
│   ├── ParallelCompressAPI.h
│   └── ParallelCompressor.h
├── ParallelCompressExample
├── SimpleTest
├── README.md
├── CHANGELOG.md
└── DOC/
```

---

## Troubleshooting

**Build fails on one platform**:
- Check Actions logs for specific errors
- Test locally if possible
- May need CMakeLists.txt adjustments

**Release not created**:
- Ensure tag matches `v*.*.*` pattern
- Check all build jobs succeeded
- Verify repository permissions

**Tests fail**:
- Tests continue with `continue-on-error: true`
- Check logs for details
- Fix and create patch release

---

## Monitoring

- **Builds**: Actions tab
- **Releases**: Releases page
- **Downloads**: Release asset stats

---

For detailed information, see [DOC/DEVELOPER_GUIDE.md](../DOC/DEVELOPER_GUIDE.md).
