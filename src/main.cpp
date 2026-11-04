// infinite_undiscovery - ReXGlue Recompiled Project

#include "generated/default/infinite_undiscovery_init.h"
#include "build_config.h"

#include "infinite_undiscovery_app.h"
#include "orb_visibility_fix.h"
#include "fiber_lr.h"
#include "frame_counter_hook.h"
#include "frame_rate.h"
#include "menu_pause_hook.h"
#include "save_backup_hook.h"
#if IU_ENABLE_TIMING_EXPERIMENT
#include "frame_timing_experiment.h"
#endif
#if IU_ENABLE_DIAGNOSTICS
#include "vesplume_diag.h"
#include "orb_move_diag.h"
#include "iu_trace_points.h"
#endif

REXCVAR_DEFINE_BOOL(asset_setup, false, "Content",
                   "Show Asset Setup Wizard even when assets are configured")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(profile_manager, false, "Content",
                   "Open Content Profile Manager")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_STRING(restart_profile, "", "Content", "Internal restart content profile")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_STRING(restart_wait_handle, "", "Content", "Internal restart process handoff")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_STRING(restore_backup, "", "Content", "Internal backup selected for startup restore")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

#if IU_ENABLE_DIAGNOSTICS
REXCVAR_DEFINE_BOOL(community_debug, true, "Debug", "Enable Infinite Undiscovery Community Debug overlay for recovery tools");

REXCVAR_DEFINE_BOOL(pso_prewarm, true, "GPU", "Wait for in-flight PSO creations at startup before resuming guest");
REXCVAR_DEFINE_BOOL(pso_telemetry, false, "Debug", "Log runtime PSO creation and frame miss telemetry");
#endif

REX_DEFINE_APP(infinite_undiscovery, InfiniteUndiscoveryApp::Create)
