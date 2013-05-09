#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>

#include <guestfs.h>

#define HFSPLUS_APPLIANCE "~/Hacking/hfsplus-appliance"

void die(const char *msg) {
	fprintf(stderr, "Error: %s\n", msg);
	exit(-1);
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s IMAGE MOUNTPOINT\n", argv[0]);
		exit(-2);
	}
	char *img = argv[1], *mp = argv[2];
	
	guestfs_h *g = guestfs_create();
	if (!g)
		exit(-3);
	// Don't kill qemu when we daemonize
	if (guestfs_set_recovery_proc(g, 0) == -1)
		exit(-3);
	
	const char *app = getenv("HFSPLUS_APPLIANCE");
	if (!app)
		app = HFSPLUS_APPLIANCE;
	wordexp_t exp;
	if (wordexp(app, &exp, 0))
		die("wordexp");
	if (guestfs_set_path(g, exp.we_wordv[0]) == -1)
		exit(-3);
	wordfree(&exp);
	
	if (guestfs_add_drive(g, img) == -1)
		exit(-3);
	if (guestfs_launch(g) == -1)
		exit(-3);
	if (guestfs_mount_vfs(g, "rw", "ufsd", "/dev/sda", "/") == -1)
		exit(-3);	
	
	if (guestfs_umask(g, 0) == -1)
		exit(-3);
	
	if (guestfs_mount_local(g, mp, -1) == -1)
		exit(-3);
	if (daemon(0, 0))
		die("daemon");
  	if (guestfs_mount_local_run(g) == -1)
		exit(-3);
	
	guestfs_umount(g, "/");
	guestfs_close(g);
	return 0;
}

