#include "threads.hpp"
#include "watchdog.hpp"
#include "system_health.hpp"

#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <errno.h>
#include <stdint.h>

// We're doing these with the atomic lib to avoid any possible data tearing the compiler may do
atomic_t boot_count = ATOMIC_INIT(0);
atomic_t op_status = ATOMIC_INIT(0);

LOG_MODULE_REGISTER(sys_health, LOG_LEVEL_DBG);

/* How often System Health drains heartbeats and re-evaluates the roster. */
static constexpr int WatchdogPollMs = 500;

/**
 * @brief Ensures the required directories exist for thread use
 * @return 0 for success, or ret from fs_mkdir for failures
 */
int ensure_directories()
{
	// Check fs is mounted for safety
	struct fs_dirent entry;
	int ret = fs_stat("/lfs", &entry);
	if (ret < 0) {
		LOG_ERR("storage not mounted at %s: %d", "/lfs", ret);
		return ret;
	}

	// Add directories here
	// fs_mkdir is POSIX style, so you need to add them individually
	const char *dirs[] = {
		"/lfs/payload",    "/lfs/payload/outbox",  "/lfs/dfa",
		"/lfs/dfa/outbox", "/lfs/communication",   "/lfs/communication/outbox",
		"/lfs/commands",   "/lfs/commands/outbox", "/lfs/syshealth",
	};

	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
		ret = fs_mkdir(dirs[i]);
		if (ret < 0 && ret != -EEXIST) {
			return ret;
		}
	}

	return 0;
}

/**
 * @brief Increments the boot count in NOR flash. Sets the boot_count
 * @return 0 for success, or ret from any fs_ operations if errors
 */
int increment_boot_count()
{
	// TODO: find a way to add the "X reboots in the last Y minutes" fault checking
	static constexpr const char *boot_count_path = "/lfs/boot_count.bin";

	struct fs_dirent entry;
	int ret = fs_stat(boot_count_path, &entry);
	if (ret < 0 && ret != -ENOENT) {
		LOG_ERR("fs_stat %s failed: %d", boot_count_path, ret);
		return ret;
	}
	if (ret == -ENOENT) {
		LOG_WRN("%s did not exist, starting boot count at 0", boot_count_path);
	}

	struct fs_file_t file;
	fs_file_t_init(&file);

	ret = fs_open(&file, boot_count_path, FS_O_CREATE | FS_O_RDWR);
	if (ret < 0) {
		LOG_ERR("fs_open %s failed: %d", boot_count_path, ret);
		return ret;
	}

	uint32_t stored_boot_count = 0;
	ssize_t bytes = fs_read(&file, &stored_boot_count, sizeof(stored_boot_count));
	if (bytes < 0) {
		LOG_ERR("fs_read %s failed: %zd", boot_count_path, bytes);
		fs_close(&file);
		return (int)bytes;
	}

	stored_boot_count++;
	LOG_INF("boot count: %u", stored_boot_count);

	ret = fs_seek(&file, 0, FS_SEEK_SET);
	if (ret < 0) {
		LOG_ERR("fs_seek %s failed: %d", boot_count_path, ret);
		fs_close(&file);
		return ret;
	}

	bytes = fs_write(&file, &stored_boot_count, sizeof(stored_boot_count));
	if (bytes < 0) {
		LOG_ERR("fs_write %s failed: %zd", boot_count_path, bytes);
		fs_close(&file);
		return (int)bytes;
	}
	if (bytes != (ssize_t)sizeof(stored_boot_count)) {
		LOG_ERR("fs_write %s short write: %zd", boot_count_path, bytes);
		fs_close(&file);
		return -EIO;
	}

	ret = fs_close(&file);
	if (ret < 0) {
		LOG_ERR("fs_close %s failed: %d", boot_count_path, ret);
		return ret;
	}

	atomic_set(&boot_count, (atomic_val_t)stored_boot_count);
	return 0;
}

/**
 * @brief main function for System Health. Runs on boot, initializes everything, and manages
 * watchdogs
 */
void system_health_entry(void *p1, void *p2, void *p3)
{
	LOG_INF("System Health: booting, starting all threads...");

	int ret = ensure_directories();
	if (ret < 0) {
		LOG_ERR("ensure_directories failed: %d", ret);
		return;
	}

	ret = increment_boot_count();
	if (ret < 0) {
		LOG_ERR("increment_boot_count failed: %d", ret);
		return;
	}

	LOG_INF("Boot count: %d", atomic_get(&boot_count));

	/* Starting a thread is its watchdog registration: arm first, then start.
	 * (epoch_ms, max_missed_cycles, startup_grace_ms) */
	health::watchdog.arm(health::MonitoredThread::Payload, 10000, 3, 15000);
	k_thread_start(payload_thread_id);

	// health::watchdog.arm(health::MonitoredThread::Dfa, 1000, 3, 2000);
	// k_thread_start(dfa_thread_id);
	// health::watchdog.arm(health::MonitoredThread::Comms, 600000, 2, 10000);
	// k_thread_start(comms_thread_id);
	// health::watchdog.arm(health::MonitoredThread::Commands, 1000, 3, 2000);
	// k_thread_start(commands_thread_id);

	while (1) {
		health::watchdog.poll();
		k_sleep(K_MSEC(WatchdogPollMs));
	}
}

K_THREAD_DEFINE(sys_health_id, ThreadConfig::DefaultStackSize, system_health_entry, NULL, NULL,
		NULL, ThreadConfig::SysHealthPriority, 0, 0);
