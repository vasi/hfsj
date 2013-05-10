#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <wordexp.h>

#include <guestfs.h>

#define ENV_APPLIANCE "HFSPLUS_APPLIANCE"
#define DRIVER "ufsd"

static int hfsplus_clean(const char *path);

static void die(const char *msg) {
	fprintf(stderr, "Error: %s\n", msg);
	exit(-1);
}

int main(int argc, char *argv[]) {
	struct guestfs_mount_local_argv options = { .bitmask = 0 };
	int usage = 0;
	int c;
	while ((c = getopt(argc, argv, "ho:")) != -1) {
		switch (c) {
			case 'o':
				if (options.bitmask & GUESTFS_MOUNT_LOCAL_OPTIONS_BITMASK)
					die("Too many options");
				options.bitmask |= GUESTFS_MOUNT_LOCAL_OPTIONS_BITMASK;
				options.options = optarg;
				break;
			case 'h':
				usage = 1;
				break;
			default:
				die("Unknown argument");
		}
	}
	if (usage || argc - optind != 2) {
		fprintf(stderr, "Usage: %s [-o OPTIONS] IMAGE MOUNTPOINT\n", argv[0]);
		exit(-2);
	}
	char *img = argv[optind++], *mp = argv[optind++];
	
	guestfs_h *g = guestfs_create();
	if (!g)
		exit(-3);
	// Don't kill qemu when we daemonize
	if (guestfs_set_recovery_proc(g, 0) == -1)
		exit(-3);
	
	const char *app = getenv(ENV_APPLIANCE);
	if (app) {
		wordexp_t exp;
		if (wordexp(app, &exp, 0))
			die("wordexp");
		if (guestfs_set_path(g, exp.we_wordv[0]) == -1)
			exit(-3);
		wordfree(&exp);
	}
	
	if (!hfsplus_clean(img))
		die("Volume not unmounted cleanly");
	if (guestfs_add_drive(g, img) == -1)
		exit(-3);
	if (guestfs_launch(g) == -1)
		exit(-3);
	if (guestfs_mount_vfs(g, "rw", DRIVER, "/dev/sda", "/") == -1)
		exit(-3);	
	
	if (guestfs_umask(g, 0) == -1)
		exit(-3);
	
	if (guestfs_mount_local_argv(g, mp, &options) == -1)
		exit(-3);
	if (daemon(0, 0))
		die("daemon");
  	if (guestfs_mount_local_run(g) == -1)
		exit(-3);
	
	guestfs_umount(g, "/");
	guestfs_close(g);
	return 0;
}

static int hfsplus_clean(const char *path) {
	int fd = open(path, O_RDONLY);
	char hdr[8];
	if (lseek(fd, 1024, SEEK_SET) == -1)
		die("seek");
	if (read(fd, hdr, 8) != 8)
		die("Can't read volume header");
	close(fd);
	
	if (bcmp(hdr, "H+", 2) && bcmp(hdr, "HX", 2))
		die("This doesn't look like an HFS+ volume");
	
	int attrs = hdr[7] + (hdr[6] << 8) + (hdr[5] << 16) + (hdr[4] << 24);
	int b_unmounted = 1 << 8, b_inconsistent = 1 << 11, b_locked = 1 << 15;
	
	if (attrs & b_locked)
		die("HFS+ volume is locked");
	return (attrs & b_unmounted) && !(attrs & b_inconsistent);
}

