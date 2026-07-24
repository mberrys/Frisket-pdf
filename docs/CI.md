# CI and diagnostic artifacts

Pull requests to `master` currently run the **Linux** and **Windows**
build-and-test jobs. **macOS** CI (build + `ctest`) is required for V1 platform
parity and is tracked in [MIC-336](https://linear.app/mbx2/issue/MIC-336) /
[docs/PLATFORM_SUPPORT.md](PLATFORM_SUPPORT.md). Packaging artifacts are produced
only for `master` pushes and manual workflow runs.

When a Windows test run fails, GitHub Actions uploads its CTest logs as the
`windows-test-logs` artifact. Store any intentional, long-lived diagnostic
artifact in the related issue or release attachment; do not add local build or
test output to the repository.

## Updating pinned workflow dependencies

Workflow actions, vcpkg, and binary packaging tools are pinned to exact
revisions or SHA-256 checksums. To refresh a pin:

1. Review the upstream release notes and commit diff.
2. Update the revision or versioned asset URL and its SHA-256 in the workflow.
3. Run the affected workflow manually and inspect its logs and generated
   artifact.
4. Record the reviewed version in the commit or pull request description.

Do not replace a pin with a moving tag such as `main`, `latest`, or `continuous`.
