/* egcs_eh_shim.c - lets the ORIGINAL rc_i386.so (egcs 1.1.2 / gcc 2.95) load on a current system.
 *
 * Its C++ try blocks call __start_cp_handler(), which the old Linux libgcc/libstdc++ used to
 * export and current ones don't, so the dynamic loader refuses the library. The function
 * returns the thread's exception-handling context; nothing is ever thrown in the paths that
 * use it (PREFS_KeyLoopUp*), so a zeroed context is enough for running the original as the
 * behavior reference. Test-rig use only: never part of the rebuild.
 *
 * Build: gcc -m32 -shared -fPIC -O2 -o egcs_eh_shim.so egcs_eh_shim.c
 * Use:   LD_PRELOAD=/path/egcs_eh_shim.so ./hlds_linux -game rockcrowbar ...
 */
static void *g_context[64];

void *__start_cp_handler(void)
{
	return g_context;
}
