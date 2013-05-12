#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <guestfs.h>

#define CONFFILE "/etc/hfsj.conf"
#define DRIVER "ufsd"

static char *appliance_path = NULL;

static int hfsplus_clean(const char *path);
static void set_appliance(guestfs_h *g);

static void die(const char *msg) {
	fprintf(stderr, "Error: %s\n", msg);
	exit(-1);
}

int main(int argc, char *argv[]) {
	struct guestfs_mount_local_argv options = { .bitmask = 0 };
	int usage = 0;
	int c;
	while ((c = getopt(argc, argv, "ho:a:")) != -1) {
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
			case 'a':
				appliance_path = optarg;
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
	set_appliance(g);
	
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
	if (fd == -1)
		die("Can't open device");
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

static void set_appliance(guestfs_h *g) {
	// Try an argument
	if (appliance_path) {
		guestfs_set_path(g, appliance_path);
		return;
	}	
	
	// Try a conf file
	char buf[512];
	FILE *f = fopen(CONFFILE, "r");
	if (!f && errno == ENOENT) {
		return;
	} else if (!f) {
		fprintf(stderr, "Can't read conffile!\n");
	} else {
		while (fgets(buf, sizeof(buf), f)) {
			const char *pref = "APPLIANCE=";
			if (strncmp(buf, pref, strlen(pref)) != 0)
				continue;
		
			char *appliance = buf + strlen(pref);
			char *eol = strchr(appliance, '\n');
			if (eol)
				*eol = '\0';
		
			guestfs_set_path(g, appliance);
			break;
		}
		fclose(f);
	}
}

