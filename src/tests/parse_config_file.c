/* liblxcapi
 *
 * Copyright © 2017 Christian Brauner <christian.brauner@ubuntu.com>.
 * Copyright © 2017 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <lxc/lxccontainer.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>

#include "conf.h"
#include "confile_utils.h"
#include "lxc/state.h"
#include "lxctest.h"
#include "utils.h"

static int set_get_compare_clear_save_load(struct lxc_container *c,
					   const char *key, const char *value,
					   const char *config_file,
					   bool compare)
{
	char retval[4096] = {0};
	int ret;

	if (!c->set_config_item(c, key, value)) {
		lxc_error("failed to set config item \"%s\" to \"%s\"\n", key,
			  value);
		return -1;
	}

	ret = c->get_config_item(c, key, retval, sizeof(retval));
	if (ret < 0) {
		lxc_error("failed to get config item \"%s\"\n", key);
		return -1;
	}

	if (compare) {
		ret = strcmp(retval, value);
		if (ret != 0) {
			lxc_error(
			    "expected value \"%s\" and retrieved value \"%s\" "
			    "for config key \"%s\" do not match\n",
			    value, retval, key);
			return -1;
		}
	}

	if (config_file) {
		if (!c->save_config(c, config_file)) {
			lxc_error("%s\n", "failed to save config file");
			return -1;
		}

		c->clear_config(c);
		c->lxc_conf = NULL;

		if (!c->load_config(c, config_file)) {
			lxc_error("%s\n", "failed to load config file");
			return -1;
		}
	}

	if (!c->clear_config_item(c, key)) {
		lxc_error("failed to clear config item \"%s\"\n", key);
		return -1;
	}

	c->clear_config(c);
	c->lxc_conf = NULL;

	return 0;
}

static int set_and_clear_complete_netdev(struct lxc_container *c)
{
	if (!c->set_config_item(c, "lxc.net.1.type", "veth")) {
		lxc_error("%s\n", "lxc.net.1.type");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.ipv4.address", "10.0.2.3/24")) {
		lxc_error("%s\n", "lxc.net.1.ipv4.address");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.ipv4.gateway", "10.0.2.2")) {
		lxc_error("%s\n", "lxc.net.1.ipv4.gateway");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.ipv4.gateway", "auto")) {
		lxc_error("%s\n", "lxc.net.1.ipv4.gateway");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.ipv4.gateway", "dev")) {
		lxc_error("%s\n", "lxc.net.1.ipv4.gateway");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.ipv6.address",
				"2003:db8:1:0:214:1234:fe0b:3596/64")) {
		lxc_error("%s\n", "lxc.net.1.ipv6.address");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.ipv6.gateway",
				"2003:db8:1:0::1")) {
		lxc_error("%s\n", "lxc.net.1.ipv6.gateway");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.ipv6.gateway", "auto")) {
		lxc_error("%s\n", "lxc.net.1.ipv6.gateway");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.ipv6.gateway", "dev")) {
		lxc_error("%s\n", "lxc.net.1.ipv6.gateway");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.flags", "up")) {
		lxc_error("%s\n", "lxc.net.1.flags");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.link", "br0")) {
		lxc_error("%s\n", "lxc.net.1.link");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.veth.pair", "bla")) {
		lxc_error("%s\n", "lxc.net.1.veth.pair");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.veth.ipv4.route", "192.0.2.1/32")) {
		lxc_error("%s\n", "lxc.net.1.veth.ipv4.route");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.veth.ipv6.route", "2001:db8::1/128")) {
		lxc_error("%s\n", "lxc.net.1.veth.ipv6.route");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.hwaddr",
				"52:54:00:80:7a:5d")) {
		lxc_error("%s\n", "lxc.net.1.hwaddr");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.net.1.mtu", "2000")) {
		lxc_error("%s\n", "lxc.net.1.mtu");
		return -1;
	}

	if (!c->clear_config_item(c, "lxc.net.1")) {
		lxc_error("%s", "failed to clear \"lxc.net.1\"\n");
		return -1;
	}

	c->clear_config(c);
	c->lxc_conf = NULL;

	return 0;
}

static int set_invalid_netdev(struct lxc_container *c) {
	if (c->set_config_item(c, "lxc.net.0.asdf", "veth")) {
		lxc_error("%s\n", "lxc.net.0.asdf should be invalid");
		return -1;
	}

	if (c->set_config_item(c, "lxc.net.2147483647.type", "veth")) {
		lxc_error("%s\n", "lxc.net.2147483647.type should be invalid");
		return -1;
	}

	if (c->set_config_item(c, "lxc.net.0.", "veth")) {
		lxc_error("%s\n", "lxc.net.0. should be invalid");
		return -1;
	}

	c->clear_config(c);
	c->lxc_conf = NULL;

	return 0;
}

int test_idmap_parser(void)
{
	size_t i;
	struct idmap_check {
		bool is_valid;
		const char *idmap;
	};
	static struct idmap_check idmaps[] = {
	    /* valid idmaps */
	    { true, "u 0 0 1"                           },
	    { true, "g 0 0 1"                           },
	    { true, "u 1 100001 999999999"              },
	    { true, "g 1 100001 999999999"              },
	    { true, "u 0 0 0"                           },
	    { true, "g 0 0 0"                           },
	    { true, "u 1000 165536 65536"               },
	    { true, "g 999 999 1"                       },
	    { true, "u    0		5000	100000" },
	    { true, "g		577	789 5"          },
	    { true, "u 65536 65536 1	"               },
	    /* invalid idmaps */
	    { false, "1u 0 0 0"                         },
	    { false, "1g 0 0 0a"                        },
	    { false, "1 u 0 0 0"                        },
	    { false, "1g 0 0 0 1"                       },
	    { false, "1u a0 b0 c0 d1"                   },
	    { false, "1g 0 b0 0 d1"                     },
	    { false, "1u a0 0 c0 1"                     },
	    { false, "g -1 0 -10"                       },
	    { false, "a 1 0 10"                         },
	    { false, "u 1 1 0 10"                       },
	    { false, "g 1 0 10	 z "                    },
	};

	for (i = 0; i < sizeof(idmaps) / sizeof(struct idmap_check); i++) {
		unsigned long hostid, nsid, range;
		char type;
		int ret;
		ret = parse_idmaps(idmaps[i].idmap, &type, &nsid, &hostid,
				   &range);
		if ((ret < 0 && idmaps[i].is_valid) ||
		    (ret == 0 && !idmaps[i].is_valid)) {
			lxc_error("failed to parse idmap \"%s\"\n",
				  idmaps[i].idmap);
			return -1;
		}
	}

	return 0;
}

static int set_get_compare_clear_save_load_network(
    struct lxc_container *c, const char *key, const char *value,
    const char *config_file, bool compare, const char *network_type)
{
	char retval[4096] = {0};
	int ret;

	if (!c->set_config_item(c, "lxc.net.0.type", network_type)) {
		lxc_error("%s\n", "lxc.net.0.type");
		return -1;
	}

	if (!c->set_config_item(c, key, value)) {
		lxc_error("failed to set config item \"%s\" to \"%s\"\n", key,
			  value);
		return -1;
	}

	ret = c->get_config_item(c, key, retval, sizeof(retval));
	if (ret < 0) {
		lxc_error("failed to get config item \"%s\"\n", key);
		return -1;
	}

	if (compare) {
		ret = strcmp(retval, value);
		if (ret != 0) {
			lxc_error(
			    "expected value \"%s\" and retrieved value \"%s\" "
			    "for config key \"%s\" do not match\n",
			    value, retval, key);
			return -1;
		}
	}

	if (config_file) {
		if (!c->save_config(c, config_file)) {
			lxc_error("%s\n", "failed to save config file");
			return -1;
		}

		c->clear_config(c);
		c->lxc_conf = NULL;

		if (!c->load_config(c, config_file)) {
			lxc_error("%s\n", "failed to load config file");
			return -1;
		}
	}

	if (!c->clear_config_item(c, key)) {
		lxc_error("failed to clear config item \"%s\"\n", key);
		return -1;
	}

	if (!c->clear_config_item(c, "lxc.net.0.type")) {
		lxc_error("%s\n", "lxc.net.0.type");
		return -1;
	}

	c->clear_config(c);
	c->lxc_conf = NULL;

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	struct lxc_container *c;
	int fd = -1, fret = EXIT_FAILURE;
	char tmpf[] = "lxc-parse-config-file-XXXXXX";
	char retval[4096] = {0};

	fd = lxc_make_tmpfile(tmpf, false);
	if (fd < 0) {
		lxc_error("%s\n", "Could not create temporary file");
		exit(fret);
	}
	close(fd);

	c = lxc_container_new(tmpf, NULL);
	if (!c) {
		lxc_error("%s\n", "Failed to create new container");
		exit(EXIT_FAILURE);
	}

	if (set_get_compare_clear_save_load(c, "lxc.arch", "x86_64", tmpf,
					    true) < 0) {
		lxc_error("%s\n", "lxc.arch");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.pty.max", "1000", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.pty.max");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.tty.max", "4", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.tty.max");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.tty.dir", "not-dev", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.tty.dir");
		goto non_test_error;
	}

	ret = set_get_compare_clear_save_load(c, "lxc.apparmor.profile", "unconfined", tmpf, true);
#if HAVE_APPARMOR
	if (ret < 0)
#else
	if (ret == 0)
#endif
	{
		lxc_error("%s\n", "lxc.apparmor.profile");
		goto non_test_error;
	}

	ret = set_get_compare_clear_save_load(c, "lxc.apparmor.allow_incomplete", "1", tmpf, true);
#if HAVE_APPARMOR
	if (ret < 0)
#else
	if (ret == 0)
#endif
	{
		lxc_error("%s\n", "lxc.apparmor.allow_incomplete");
		goto non_test_error;
	}

	ret = set_get_compare_clear_save_load(c, "lxc.selinux.context", "system_u:system_r:lxc_t:s0:c22", tmpf, true);
#if HAVE_SELINUX
	if (ret < 0)
#else
	if (ret == 0)
#endif
	{
		lxc_error("%s\n", "lxc.selinux.context");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.cgroup.cpuset.cpus",
					    "1-100", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.cgroup.cpuset.cpus");
		goto non_test_error;
	}

	if (!c->set_config_item(c, "lxc.cgroup.cpuset.cpus", "1-100")) {
		lxc_error("%s\n", "failed to set config item \"lxc.cgroup.cpuset.cpus\" to \"1-100\"");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.cgroup.memory.limit_in_bytes", "123456789")) {
		lxc_error("%s\n", "failed to set config item \"lxc.cgroup.memory.limit_in_bytes\" to \"123456789\"");
		return -1;
	}

	if (!c->get_config_item(c, "lxc.cgroup", retval, sizeof(retval))) {
		lxc_error("%s\n", "failed to get config item \"lxc.cgroup\"");
		return -1;
	}

	c->clear_config(c);
	c->lxc_conf = NULL;

	/* lxc.idmap
	 * We can't really save the config here since save_config() wants to
	 * chown the container's directory but we haven't created an on-disk
	 * container. So let's test set-get-clear.
	 */
	if (set_get_compare_clear_save_load(c, "lxc.idmap", "u 0 100000 1000000000", NULL, false) < 0) {
		lxc_error("%s\n", "lxc.idmap");
		goto non_test_error;
	}

	if (!c->set_config_item(c, "lxc.idmap", "u 1 100000 10000000")) {
		lxc_error("%s\n", "failed to set config item \"lxc.idmap\" to \"u 1 100000 10000000\"");
		return -1;
	}

	if (!c->set_config_item(c, "lxc.idmap", "g 1 100000 10000000")) {
		lxc_error("%s\n", "failed to set config item \"lxc.idmap\" to \"g 1 100000 10000000\"");
		return -1;
	}

	if (!c->get_config_item(c, "lxc.idmap", retval, sizeof(retval))) {
		lxc_error("%s\n", "failed to get config item \"lxc.idmap\"");
		return -1;
	}

	c->clear_config(c);
	c->lxc_conf = NULL;

	if (set_get_compare_clear_save_load(c, "lxc.log.level", "DEBUG", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.log.level");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.log.file", "/some/path", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.log.file");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.mount.fstab", "/some/path", NULL, true) < 0) {
		lxc_error("%s\n", "lxc.mount.fstab");
		goto non_test_error;
	}

	/* lxc.mount.auto
	 * Note that we cannot compare the values since the getter for
	 * lxc.mount.auto does not preserve ordering.
	 */
	if (set_get_compare_clear_save_load(c, "lxc.mount.auto", "proc:rw sys:rw cgroup-full:rw", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.mount.auto");
		goto non_test_error;
	}

	/* lxc.mount.entry
	 * Note that we cannot compare the values since the getter for
	 * lxc.mount.entry appends newlines.
	 */
	if (set_get_compare_clear_save_load(c, "lxc.mount.entry", "/dev/dri dev/dri none bind,optional,create=dir", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.mount.entry");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.rootfs.path", "/some/path", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.rootfs.path");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.rootfs.mount", "/some/path", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.rootfs.mount");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.rootfs.options", "ext4,discard", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.rootfs.options");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.uts.name", "the-shire", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.uts.name");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(
		c, "lxc.hook.pre-start", "/some/pre-start", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.pre-start");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(
		c, "lxc.hook.pre-mount", "/some/pre-mount", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.pre-mount");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.hook.mount", "/some/mount", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.mount");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.hook.autodev", "/some/autodev", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.autodev");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.hook.start", "/some/start", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.start");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.hook.stop", "/some/stop", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.stop");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.hook.post-stop", "/some/post-stop", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.post-stop");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.hook.clone", "/some/clone", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.clone");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.hook.destroy", "/some/destroy", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.hook.destroy");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.cap.drop", "sys_module mknod setuid net_raw", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.cap.drop");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.cap.keep", "sys_module mknod setuid net_raw", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.cap.keep");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.console.path", "none", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.console.path");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.console.logfile", "/some/logfile", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.console.logfile");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.seccomp.profile", "/some/seccomp/file", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.seccomp.profile");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.autodev.tmpfs.size", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.autodev.tmpfs.size");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.autodev", "1", tmpf, true) <
	    0) {
		lxc_error("%s\n", "lxc.autodev");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.signal.halt", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.signal.halt");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.signal.reboot", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.signal.reboot");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.signal.stop", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.signal.stop");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.start.auto", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.start.auto");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.start.delay", "5", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.start.delay");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.start.order", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.start.order");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.log.syslog", "local0", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.log.syslog");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.monitor.unshare", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.monitor.unshare");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.group", "some,container,groups", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.group");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.environment", "FOO=BAR", tmpf, false) < 0) {
		lxc_error("%s\n", "lxc.environment");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.init.cmd", "/bin/bash", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.init.cmd");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.init.uid", "1000", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.init.uid");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.init.gid", "1000", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.init.gid");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.ephemeral", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.ephemeral");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.no_new_privs", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.no_new_privs");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.sysctl.net.core.somaxconn", "256", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.sysctl.net.core.somaxconn");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.proc.oom_score_adj", "10", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.proc.oom_score_adj");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.prlimit.nofile", "65536", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.prlimit.nofile");
		goto non_test_error;
	}

	if (test_idmap_parser() < 0) {
		lxc_error("%s\n", "failed to test parser for \"lxc.id_map\"");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.type", "veth", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.type");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.2.type", "none", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.2.type");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.3.type", "empty", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.3.type");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.4.type", "vlan", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.4.type");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.type", "macvlan", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.type");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.type", "ipvlan", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.type");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.1000.type", "phys", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.1000.type");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.flags", "up", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.flags");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.name", "eth0", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.name");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.link", "bla", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.link");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.macvlan.mode", "private", tmpf, true, "macvlan")) {
		lxc_error("%s\n", "lxc.net.0.macvlan.mode");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.macvlan.mode", "vepa", tmpf, true, "macvlan")) {
		lxc_error("%s\n", "lxc.net.0.macvlan.mode");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.macvlan.mode", "bridge", tmpf, true, "macvlan")) {
		lxc_error("%s\n", "lxc.net.0.macvlan.mode");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.ipvlan.mode", "l3", tmpf, true, "ipvlan")) {
		lxc_error("%s\n", "lxc.net.0.ipvlan.mode");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.ipvlan.mode", "l3s", tmpf, true, "ipvlan")) {
		lxc_error("%s\n", "lxc.net.0.ipvlan.mode");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.ipvlan.mode", "l2", tmpf, true, "ipvlan")) {
		lxc_error("%s\n", "lxc.net.0.ipvlan.mode");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.ipvlan.isolation", "bridge", tmpf, true, "ipvlan")) {
		lxc_error("%s\n", "lxc.net.0.ipvlan.isolation");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.ipvlan.isolation", "private", tmpf, true, "ipvlan")) {
		lxc_error("%s\n", "lxc.net.0.ipvlan.isolation");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.ipvlan.isolation", "vepa", tmpf, true, "ipvlan")) {
		lxc_error("%s\n", "lxc.net.0.ipvlan.isolation");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.veth.pair", "clusterfuck", tmpf, true, "veth")) {
		lxc_error("%s\n", "lxc.net.0.veth.pair");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.veth.ipv4.route", "192.0.2.1/32", tmpf, true, "veth")) {
		lxc_error("%s\n", "lxc.net.0.veth.ipv4.route");
		return -1;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.veth.ipv6.route", "2001:db8::1/128", tmpf, true, "veth")) {
		lxc_error("%s\n", "lxc.net.0.veth.ipv6.route");
		return -1;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.veth.vlan.id", "none", tmpf, false, "veth")) {
		lxc_error("%s\n", "lxc.net.0.veth.vlan.id");
		return -1;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.veth.vlan.id", "2", tmpf, true, "veth")) {
		lxc_error("%s\n", "lxc.net.0.veth.vlan.id");
		return -1;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.veth.vlan.tagged.id", "2", tmpf, true, "veth")) {
		lxc_error("%s\n", "lxc.net.0.veth.vlan.tagged.id");
		return -1;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.script.up", "/some/up/path", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.script.up");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.script.down", "/some/down/path", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.script.down");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.hwaddr", "52:54:00:80:7a:5d", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.hwaddr");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.mtu", "2000", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.mtu");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load_network(c, "lxc.net.0.vlan.id", "2", tmpf, true, "vlan")) {
		lxc_error("%s\n", "lxc.net.0.vlan.id");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.ipv4.gateway", "10.0.2.2", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.ipv4.gateway");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.ipv4.gateway", "auto", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.ipv4.gateway");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.ipv4.gateway", "dev", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.ipv4.gateway");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.ipv6.gateway", "2003:db8:1::1", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.ipv6.gateway");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.ipv6.gateway", "auto", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.ipv6.gateway");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.ipv6.gateway", "dev", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.ipv6.gateway");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.ipv4.address", "10.0.2.3/24", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.ipv4.address");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.net.0.ipv6.address", "2003:db8:1:0:214:1234:fe0b:3596/64", tmpf, true)) {
		lxc_error("%s\n", "lxc.net.0.ipv6.address");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.cgroup.dir", "lxd", tmpf, true)) {
		lxc_error("%s\n", "lxc.cgroup.dir");
		goto non_test_error;
	}

	if (set_and_clear_complete_netdev(c) < 0) {
		lxc_error("%s\n", "failed to clear whole network");
		goto non_test_error;
	}

	if (set_invalid_netdev(c) < 0) {
		lxc_error("%s\n", "failed to reject invalid configuration");
		goto non_test_error;
	}

	ret = set_get_compare_clear_save_load(c, "lxc.hook.version", "1", tmpf, true);
	if (ret < 0) {
		lxc_error("%s\n", "lxc.hook.version");
		goto non_test_error;
	}

	if (c->set_config_item(c, "lxc.hook.version", "2")) {
		lxc_error("%s\n", "Managed to set to set invalid config item \"lxc.hook.version\" to \"2\"");
		goto non_test_error;
	}

	if (!c->set_config_item(c, "lxc.monitor.signal.pdeath", "SIGKILL")) {
		lxc_error("%s\n", "Failed to set to set invalid config item \"lxc.monitor.signal.pdeath\" to \"SIGKILL\"");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.rootfs.managed", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.rootfs.managed");
		goto non_test_error;
	}

	if (c->set_config_item(c, "lxc.notaconfigkey", "invalid")) {
		lxc_error("%s\n", "Managed to set to set invalid config item \"lxc.notaconfigkey\" to \"invalid\"");
		return -1;
	}

	if (c->set_config_item(c, "lxc.log.file=", "./")) {
		lxc_error("%s\n", "Managed to set to set invalid config item \"lxc.log.file\" to \"./\"");
		return -1;
	}

	if (c->set_config_item(c, "lxc.hook.versionasdfsadfsadf", "1")) {
		lxc_error("%s\n", "Managed to set to set invalid config item \"lxc.hook.versionasdfsadfsadf\" to \"2\"");
		goto non_test_error;
	}

	if (set_get_compare_clear_save_load(c, "lxc.sched.core", "1", tmpf, true) < 0) {
		lxc_error("%s\n", "lxc.sched.core");
		goto non_test_error;
	}

	fret = EXIT_SUCCESS;

non_test_error:
	(void)unlink(tmpf);
	(void)rmdir(dirname(c->configfile));
	lxc_container_put(c);
	exit(fret);
}
