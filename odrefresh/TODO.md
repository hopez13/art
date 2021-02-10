# TODO (STOPSHIP until done)

1. <strike>Fix dexoptanalyzer so it can analyze boot extensions.</strike>
1. <strike>Parse apex-info-list.xml into an apex_info (to make version and location available).</strike>
1. Add a log file that tracks status of recent compilation.
1. Implement back off on trying compilation when previous attempt(s) failed.
1. Free space calculation and only attempting compilation if sufficient space.
1. Metrics for tracking issues:
   - Successful compilation of all artifacts.
   - Time limit exceeded (indicates a pathological issue, e.g. dex2oat bug, device driver bug, etc).
   - Insufficient space for compilation.
   - Compilation failure (boot extensions)
   - Compilation failure (system server)
   - Unexpected error (a setup or clean-up action failed).
1. <strike>Timeouts for pathological failures.</strike> Metrics recording for subprocess timeouts.
1. Decide and implement testing.
